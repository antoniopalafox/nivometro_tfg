#pragma once

#include "hcsr04p.h"
#include "hx711.h"
#include "esp_err.h"
#include "esp_log.h"

// Estructura de datos unificada del nivómetro (SIN VL53L0X)
typedef struct {
    // Datos de sensores
    float ultrasonic_distance_cm;    // HC-SR04P
    float weight_grams;              // HX711
    
    // Metadatos
    uint64_t timestamp_us;
    uint8_t sensor_status;           // Bits: [1]=HX711, [0]=HC-SR04P (VL53L0X eliminado)
    float battery_voltage;
    int8_t temperature_c;            // Temperatura estimada
} nivometro_data_t;

// Estructura de datos para comunicación entre componentes 
typedef struct {
    float distance_cm;        // Distancia HC-SR04P en centímetros
    float weight_kg;          // Peso HX711 en kilogramos  
    int64_t timestamp_us;     // Timestamp en microsegundos
    uint8_t sensor_status;    // Status de sensores (bits)
    float battery_voltage;    // Voltaje batería
    int temperature_c;        // Temperatura en Celsius
} sensor_data_t;

// Configuración del nivómetro 
typedef struct {
    // Pines HC-SR04P
    int hcsr04p_trigger_pin;
    int hcsr04p_echo_pin;
    float hcsr04p_cal_factor;
    
    // Pines HX711
    int hx711_dout_pin;
    int hx711_sck_pin;
    hx711_gain_t hx711_gain;
    float hx711_known_weight;
} nivometro_config_t;

// Estructura principal del nivómetro
typedef struct {
    hcsr04p_sensor_t ultrasonic;
    hx711_sensor_t scale;
    nivometro_config_t config;
    bool initialized;
} nivometro_t;

// ==============================================================================
// FUNCIONES PRINCIPALES ORIGINALES
// ==============================================================================

esp_err_t nivometro_init(nivometro_t *nivometro, const nivometro_config_t *config);
esp_err_t nivometro_read_all_sensors(nivometro_t *nivometro, nivometro_data_t *data);
esp_err_t nivometro_calibrate_all(nivometro_t *nivometro);
esp_err_t nivometro_calibrate_scale(nivometro_t *nivometro, float known_weight_g);
esp_err_t nivometro_tare_scale(nivometro_t *nivometro);
void nivometro_power_down(nivometro_t *nivometro);
void nivometro_power_up(nivometro_t *nivometro);

// Funciones de utilidad
const char* nivometro_get_sensor_status_string(uint8_t status);
bool nivometro_is_sensor_working(uint8_t status, int sensor_index);

// Función de conversión entre estructuras
void nivometro_data_to_sensor_data(const nivometro_data_t *src, sensor_data_t *dst);

// ==============================================================================
// FUNCIONES DE CALIBRACIÓN AVANZADA
// ==============================================================================

/**
 * Calibra el sensor HC-SR04P con distancia conocida configurable
 * @param nivometro Puntero al objeto nivómetro
 * @param known_distance_cm Distancia conocida en centímetros
 * @param samples Número de mediciones a promediar
 * @param tolerance_percent Tolerancia máxima aceptable (%)
 * @return Factor de calibración calculado o 0.0f si falla
 */
float nivometro_calibrate_ultrasonic(nivometro_t *nivometro, 
                                   float known_distance_cm, 
                                   int samples, 
                                   float tolerance_percent);

/**
 * Calibra el sensor HX711 con validación automática
 * @param nivometro Puntero al objeto nivómetro
 * @param known_weight_g Peso conocido en gramos
 * @param tolerance_percent Tolerancia máxima aceptable (%)
 * @return ESP_OK si la calibración es exitosa y válida
 */
esp_err_t nivometro_calibrate_scale_with_validation(nivometro_t *nivometro, 
                                                  float known_weight_g, 
                                                  float tolerance_percent);

/**
 * Verifica que los sensores estén funcionando correctamente
 * @param nivometro Puntero al objeto nivómetro
 * @return true si todos los sensores están operativos
 */
bool nivometro_verify_sensors_health(nivometro_t *nivometro);

/**
 * Obtiene el factor de calibración actual del HC-SR04P
 * @param nivometro Puntero al objeto nivómetro
 * @return Factor de calibración actual
 */
float nivometro_get_ultrasonic_calibration_factor(const nivometro_t *nivometro);

/**
 * Obtiene los parámetros de calibración actuales del HX711
 * @param nivometro Puntero al objeto nivómetro
 * @param scale_factor Puntero para retornar el factor de escala
 * @param offset Puntero para retornar el offset/tara
 */
void nivometro_get_scale_calibration_params(const nivometro_t *nivometro, 
                                          float *scale_factor, 
                                          int32_t *offset);

/**
 * Aplica factores de calibración específicos a los sensores
 * @param nivometro Puntero al objeto nivómetro
 * @param hcsr04p_factor Factor de calibración para HC-SR04P
 * @param hx711_scale Factor de escala para HX711
 * @param hx711_offset Offset/tara para HX711
 * @return ESP_OK si se aplicaron correctamente
 */
esp_err_t nivometro_apply_calibration_factors(nivometro_t *nivometro,
                                            float hcsr04p_factor,
                                            float hx711_scale,
                                            int32_t hx711_offset);

/**
 * Realiza una prueba de calibración completa
 * @param nivometro Puntero al objeto nivómetro
 * @param known_weight_g Peso conocido para HX711
 * @param known_distance_cm Distancia conocida para HC-SR04P
 * @return ESP_OK si todos los sensores se calibran correctamente
 */
esp_err_t nivometro_full_calibration_test(nivometro_t *nivometro,
                                        float known_weight_g,
                                        float known_distance_cm);