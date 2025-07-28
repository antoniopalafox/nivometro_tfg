// File: components/utils/include/utils.h
#pragma once                               // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "nivometro_sensors.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ==============================================================================
// FUNCIONES ORIGINALES DE UTILS
// ==============================================================================

void timer_manager_init(void);              // Inicializa el gestor de temporizadores para usar timer_manager_delay_ms()
void timer_manager_delay_ms(uint32_t ms);   // Retrasa la ejecución de la tarea actual 
int data_formatter_format_json(const sensor_data_t *data, char *buf, size_t bufsize);   // Serializa los datos de sensores como json en buf

// ==============================================================================
// SISTEMA DE CONTROL LED - ESTADOS FIJOS
// ==============================================================================

// Definiciones de hardware
#define LED_STATUS_PIN          GPIO_NUM_2   // LED azul integrado ESP32-WROOM-32E

// Estados del LED (6 estados diferenciados)
typedef enum {
    LED_STATE_OFF,              // Sistema apagado
    LED_STATE_NORMAL,           // Normal calibrado - parpadeo muy lento (2000ms)
    LED_STATE_WARNING,          // Normal sin calibrar - parpadeo medio (800ms)  
    LED_STATE_CALIBRATION,      // Modo calibración - parpadeo rápido (300ms)
    LED_STATE_ERROR,            // Error/fallo - parpadeo muy rápido (150ms)
    LED_STATE_SOLID_ON          // Proceso completo - encendido fijo
} led_state_t;

// Períodos fijos para cada estado (en milisegundos)
#define LED_PERIOD_NORMAL_MS        2000    // Muy lento - sistema OK
#define LED_PERIOD_WARNING_MS       800     // Medio - requiere atención
#define LED_PERIOD_CALIBRATION_MS   300     // Rápido - proceso activo
#define LED_PERIOD_ERROR_MS         150     // Muy rápido - urgente

// Funciones de control LED
void led_init(void);                        // Inicializa el GPIO del LED
void led_start_task(void);                  // Inicia la tarea de control automático del LED
void led_set_state(led_state_t state);      // Cambia el estado del LED
led_state_t led_get_state(void);            // Obtiene el estado actual del LED
void led_stop_task(void);                   // Detiene la tarea de control del LED

// ==============================================================================
// SISTEMA DE CALIBRACIÓN - GESTIÓN NVS
// ==============================================================================

// Definiciones para calibración
#define BOOT_BUTTON_PIN         GPIO_NUM_0   // Botón BOOT de la ESP32
#define CALIBRATION_DEBOUNCE_MS 50           // Anti-rebote del botón

// Estructura de datos de calibración almacenados en NVS
typedef struct {
    float hx711_scale_factor;           // Factor de escala HX711
    int32_t hx711_offset;               // Offset/tara HX711
    float hcsr04p_cal_factor;           // Factor de calibración HC-SR04P
    uint32_t magic_number;              // Número mágico para validar datos
    bool calibrated;                    // Flag de calibración válida
    float known_weight_used;            // Peso conocido usado en calibración
    float known_distance_used;          // Distancia conocida usada en calibración
    uint32_t calibration_timestamp;     // Timestamp de cuándo se calibró
} calibration_data_t;

// Funciones de gestión NVS para calibración
esp_err_t calibration_save_to_nvs(const calibration_data_t *cal_data);
esp_err_t calibration_load_from_nvs(calibration_data_t *cal_data);
esp_err_t calibration_clear_nvs(void);
bool calibration_check_and_warn(void);
esp_err_t calibration_apply_to_sensors(nivometro_t *nivometro, const calibration_data_t *cal_data);

// ==============================================================================
// DETECCIÓN BOTÓN BOOT PARA MODO CALIBRACIÓN
// ==============================================================================

// Funciones de detección del botón BOOT
void boot_button_init(void);                    // Inicializa el botón BOOT
bool boot_button_check_calibration_mode(void);  // Verifica si se debe entrar en modo calibración
void boot_button_wait_for_press(void);          // Espera a que el usuario presione BOOT

// ==============================================================================
// FUNCIONES DE VALIDACIÓN Y UTILIDADES
// ==============================================================================

// Funciones de validación para calibración
bool validate_calibration_data(const calibration_data_t *cal_data);
float calculate_error_percent(float measured, float expected);
bool is_value_in_tolerance(float measured, float expected, float tolerance_percent);

// Utilidades de tiempo y formato
uint32_t get_timestamp_seconds(void);
void format_calibration_summary(const calibration_data_t *cal_data, char *buffer, size_t buffer_size);