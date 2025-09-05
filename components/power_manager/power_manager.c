// File: components/power_manager/power_manager.c

#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <inttypes.h>  // Para PRIu32

static const char* TAG = "power_manager";

// -------------------------------------------------------------------------
// MODO REAL ACTIVADO: Detección por GPIO
// -------------------------------------------------------------------------
static bool simulation_enabled = false;  // CAMBIADO: false = modo real
// -------------------------------------------------------------------------

// Pin HW para detección de USB en modo real
#define USB_DETECT_PIN  GPIO_NUM_4  // Ajusta este pin según tu hardware

// Variables para logging de cambios de estado (opcional)
static power_source_t last_detected_source = POWER_SOURCE_UNKNOWN;
static uint32_t state_change_count = 0;

// Obtener tiempo en ms
static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void power_manager_init(void) {
    if (simulation_enabled) {
        // Código de simulación (mantenido para pruebas futuras)
        ESP_LOGI(TAG, "MODO SIMULACIÓN ACTIVADO (no se usa actualmente)");
    } else {
        // MODO REAL: configurar GPIO de detección USB
        ESP_LOGI(TAG, "==========================================");
        ESP_LOGI(TAG, "MODO DETECCIÓN REAL ACTIVADO");
        ESP_LOGI(TAG, "Pin de detección: GPIO %d", USB_DETECT_PIN);
        ESP_LOGI(TAG, "Lógica: 1 = USB conectado | 0 = Solo batería");
        ESP_LOGI(TAG, "==========================================");
        
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << USB_DETECT_PIN,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,      // Pull-up habilitado
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE
        };
        
        esp_err_t result = gpio_config(&cfg);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Error configurando GPIO %d: %s", USB_DETECT_PIN, esp_err_to_name(result));
            return;
        }
        
        // Leer estado inicial y reportar
        int initial_level = gpio_get_level(USB_DETECT_PIN);
        last_detected_source = initial_level ? POWER_SOURCE_USB : POWER_SOURCE_BATTERY;
        
        ESP_LOGI(TAG, "Estado inicial GPIO %d: %d (%s)", 
                 USB_DETECT_PIN, 
                 initial_level,
                 last_detected_source == POWER_SOURCE_USB ? "USB/Nominal" : "Batería/Ahorro");
    }
}

power_source_t power_manager_get_source(void) {
    if (simulation_enabled) {
        // Código de simulación (no se ejecuta)
        return POWER_SOURCE_USB;
    } else {
        // MODO REAL: leer GPIO directamente
        int level = gpio_get_level(USB_DETECT_PIN);
        power_source_t current_source = level ? POWER_SOURCE_USB : POWER_SOURCE_BATTERY;
        
        // Detectar y loggear cambios de estado
        if (current_source != last_detected_source && last_detected_source != POWER_SOURCE_UNKNOWN) {
            state_change_count++;
            ESP_LOGI(TAG, "CAMBIO DETECTADO #%lu: %s → %s (GPIO %d: %d)", 
                     state_change_count,
                     last_detected_source == POWER_SOURCE_USB ? "USB" : "Batería",
                     current_source == POWER_SOURCE_USB ? "USB" : "Batería",
                     USB_DETECT_PIN,
                     level);
        }
        
        last_detected_source = current_source;
        return current_source;
    }
}

bool power_manager_should_sleep(void) {
    power_source_t source = power_manager_get_source();
    bool should_sleep = (source != POWER_SOURCE_USB);
    
    // Log detallado para debugging
    ESP_LOGD(TAG, "should_sleep(): GPIO %d = %d, fuente = %s, sleep = %s",
             USB_DETECT_PIN,
             gpio_get_level(USB_DETECT_PIN),
             source == POWER_SOURCE_USB ? "USB" : "Batería",
             should_sleep ? "SÍ" : "NO");
    
    return should_sleep;
}

void power_manager_enter_deep_sleep(void) {
    // Verificar una vez más antes de entrar en sleep
    power_source_t current_source = power_manager_get_source();
    
    if (current_source == POWER_SOURCE_USB) {
        ESP_LOGW(TAG, "CANCELANDO Deep Sleep: USB conectado detectado (GPIO %d = 1)", USB_DETECT_PIN);
        return;
    }
    
    ESP_LOGI(TAG, "Entrando en deep sleep... (GPIO %d = 0, modo batería)", USB_DETECT_PIN);
    ESP_LOGI(TAG, "Configurando despertar por timer en 30 segundos");
    
    const uint64_t SLEEP_US = 30ULL * 1000000ULL;  // 30 segundos
    esp_sleep_enable_timer_wakeup(SLEEP_US);
    
    ESP_LOGI(TAG, "Iniciando deep sleep ahora...");
    esp_deep_sleep_start();
}

bool power_manager_is_usb_connected(void) {
    return (power_manager_get_source() == POWER_SOURCE_USB);
}

// Función de diagnóstico para verificar el estado del GPIO
void power_manager_debug_gpio_state(void) {
    if (!simulation_enabled) {
        int current_level = gpio_get_level(USB_DETECT_PIN);
        power_source_t source = power_manager_get_source();
        
        ESP_LOGI(TAG, "=== DEBUG GPIO ESTADO ===");
        ESP_LOGI(TAG, "Pin: GPIO %d", USB_DETECT_PIN);
        ESP_LOGI(TAG, "Nivel actual: %d", current_level);
        ESP_LOGI(TAG, "Fuente detectada: %s", source == POWER_SOURCE_USB ? "USB/Nominal" : "Batería/Ahorro");
        ESP_LOGI(TAG, "Cambios detectados: %lu", state_change_count);
        ESP_LOGI(TAG, "========================");
    }
}

// Funciones de control manual (comentadas - no necesarias en modo real)
/*
void power_manager_force_usb_simulation(void) {
    ESP_LOGW(TAG, "force_usb_simulation() no disponible en modo real GPIO");
}

void power_manager_force_battery_simulation(void) {
    ESP_LOGW(TAG, "force_battery_simulation() no disponible en modo real GPIO");
}
*/