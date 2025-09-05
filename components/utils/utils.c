// File: components/utils/utils.c

#include "utils.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"

static const char* TAG = "utils";

void timer_manager_init(void)
{
    // Inicializa hardware o recursos de temporización
    ESP_LOGI(TAG, "Timer manager inicializado");
}

void timer_manager_delay_ms(uint32_t ms)
{
    // Retrasa la tarea actual ms usando freertos
    vTaskDelay(pdMS_TO_TICKS(ms));
}

int data_formatter_format_json(const sensor_data_t *data, char *buf, size_t bufsize)
{
    // Serializa los valores de sensores en un json dentro de buf (SIN VL53L0X)
    return snprintf(buf, bufsize,
        "{\"distance_cm\":%.2f,\"weight_kg\":%.2f}",
        data->distance_cm,
        data->weight_kg
    );
}

// Variables globales para control del LED
static TaskHandle_t led_task_handle = NULL;
static led_state_t current_led_state = LED_STATE_OFF;

// Controla el LED según el estado actual
static void led_control_task(void *pvParameters) {
    bool led_physical_state = false;
    uint32_t delay_ms;
       
    while (1) {
        switch (current_led_state) {
            case LED_STATE_NORMAL:
                // 🟢 Normal calibrado - parpadeo muy lento
                delay_ms = LED_PERIOD_NORMAL_MS;
                led_physical_state = !led_physical_state;
                gpio_set_level(LED_STATUS_PIN, led_physical_state);
                break;
                
            case LED_STATE_WARNING:
                // 🟡 Normal sin calibrar - parpadeo medio
                delay_ms = LED_PERIOD_WARNING_MS;
                led_physical_state = !led_physical_state;
                gpio_set_level(LED_STATUS_PIN, led_physical_state);
                break;
                
            case LED_STATE_CALIBRATION:
                // 🔵 Modo calibración - parpadeo rápido
                delay_ms = LED_PERIOD_CALIBRATION_MS;
                led_physical_state = !led_physical_state;
                gpio_set_level(LED_STATUS_PIN, led_physical_state);
                break;
                
            case LED_STATE_ERROR:
                // 🔴 Error/fallo - parpadeo muy rápido
                delay_ms = LED_PERIOD_ERROR_MS;
                led_physical_state = !led_physical_state;
                gpio_set_level(LED_STATUS_PIN, led_physical_state);
                break;
                
            case LED_STATE_SOLID_ON:
                // ⚪ Proceso completo - encendido fijo
                delay_ms = 100;  // Verificar cada 100ms pero mantener encendido
                gpio_set_level(LED_STATUS_PIN, 1);
                break;
                
            case LED_STATE_OFF:
            default:
                // ⚫ Sistema apagado
                delay_ms = 100;  // Verificar cada 100ms pero mantener apagado
                gpio_set_level(LED_STATUS_PIN, 0);
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void led_init(void) {
    // Resetear completamente el pin primero
    gpio_reset_pin(LED_STATUS_PIN);
    
    // Configurar GPIO del LED
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_STATUS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t result = gpio_config(&led_config);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando GPIO %d: %s", LED_STATUS_PIN, esp_err_to_name(result));
        return;
    }
    
    // LED inicialmente apagado
    gpio_set_level(LED_STATUS_PIN, 0);
    current_led_state = LED_STATE_OFF;
    
}
void led_start_task(void) {
    if (led_task_handle == NULL) {
        xTaskCreate(led_control_task, "led_control", 2048, NULL, 1, &led_task_handle);
        ESP_LOGI(TAG, "Tarea de control LED iniciada");
    }
}
// Cambia el estado del LED y actualiza la tarea de control
void led_set_state(led_state_t state) {
    current_led_state = state;
    const char* state_names[] = {
        "⚫ APAGADO", "🟢 NORMAL", "🟡 ADVERTENCIA", 
        "🔵 CALIBRACIÓN", "🔴 ERROR", "⚪ ENCENDIDO"
    };
    ESP_LOGI(TAG, "LED estado cambiado: %s", state_names[state]);
}
led_state_t led_get_state(void) {
    return current_led_state;
}
void led_stop_task(void) {
    if (led_task_handle != NULL) {
        vTaskDelete(led_task_handle);
        led_task_handle = NULL;
        gpio_set_level(LED_STATUS_PIN, 0);
        
    }
}

esp_err_t calibration_save_to_nvs(const calibration_data_t *cal_data) {
    nvs_handle_t nvs_handle;
    esp_err_t err;
    
    // Abrir NVS en modo escritura
    err = nvs_open(CALIBRATION_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo abrir NVS para escritura: %s", esp_err_to_name(err));
        return err;
    }
    
    // Guardar datos
    err = nvs_set_blob(nvs_handle, "cal_data", cal_data, sizeof(calibration_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error guardando calibración: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }
    
    // Confirmar escritura
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Datos de calibración guardados en NVS");
    } /*else {
        ESP_LOGE(TAG, "Error confirmando escritura: %s", esp_err_to_name(err));
    }*/
    
    return err;
}

esp_err_t calibration_load_from_nvs(calibration_data_t *cal_data) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(CALIBRATION_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo abrir NVS para calibración: %s", esp_err_to_name(err));
        return err;
    }
    
    // Leer datos de calibración
    size_t required_size = sizeof(calibration_data_t);
    err = nvs_get_blob(nvs_handle, "cal_data", cal_data, &required_size);
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        // Validar datos
        if (cal_data->magic_number == CALIBRATION_MAGIC_NUMBER && cal_data->calibrated) {
            ESP_LOGI(TAG, "Datos de calibración cargados desde NVS");
            ESP_LOGI(TAG, "HX711 - Escala: %.6f, Offset: %ld", 
                     cal_data->hx711_scale_factor, cal_data->hx711_offset);
            ESP_LOGI(TAG, "HC-SR04P - Factor: %.6f", cal_data->hcsr04p_cal_factor);
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Datos de calibración corruptos en NVS");
            return ESP_ERR_INVALID_CRC;
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No se encontraron datos de calibración en NVS");
        return ESP_ERR_NOT_FOUND;
    } else {
        ESP_LOGE(TAG, "Error leyendo calibración de NVS: %s", esp_err_to_name(err));
        return err;
    }
}

esp_err_t calibration_all_nvs_partition(void) {
    
    // Borrar completamente toda la partición NVS
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Toda la partición NVS borrada");
        
        // Reinicializar NVS completamente
        err = nvs_flash_init();
    } else {
        ESP_LOGE(TAG, "Error borrando partición NVS: %s", esp_err_to_name(err));
    }
    
    return err;
}

bool calibration_check_and_warn(void) {
    calibration_data_t cal_data = {0};
    esp_err_t err = calibration_load_from_nvs(&cal_data);
    
    if (err == ESP_OK) {
        // Verificar que los valores no sean cero
        if (cal_data.hx711_scale_factor == 0.0f || 
            cal_data.hcsr04p_cal_factor == 0.0f) {
            
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "ADVERTENCIA: DATOS A CERO");
            ESP_LOGW(TAG, "========================================");
            ESP_LOGW(TAG, "Los datos de calibración contienen valores cero");
            ESP_LOGW(TAG, "Para obtener mediciones precisas:");
            ESP_LOGW(TAG, "1. Reinicia manteniendo BOOT presionado");
            ESP_LOGW(TAG, "2. Completa el proceso de calibración");
            ESP_LOGW(TAG, "========================================");
            return false;
        }
        
        ESP_LOGI(TAG, "Calibración válida encontrada");
        return true;
        
    } else {
        ESP_LOGW(TAG, "========================================");
        ESP_LOGW(TAG, "ADVERTENCIA: SIN CALIBRACIÓN");
        ESP_LOGW(TAG, "========================================");
        ESP_LOGW(TAG, "No se encontraron datos de calibración");
        ESP_LOGW(TAG, "Los sensores usarán valores por defecto");
        ESP_LOGW(TAG, "Para calibrar correctamente:");
        ESP_LOGW(TAG, "1. Reinicia manteniendo BOOT presionado");
        ESP_LOGW(TAG, "2. Sigue las instrucciones de calibración");
        ESP_LOGW(TAG, "========================================");
        return false;
    }
}

esp_err_t calibration_apply_to_sensors(nivometro_t *nivometro, const calibration_data_t *cal_data) {
    // Aplicar calibración HX711
    nivometro->scale.scale = cal_data->hx711_scale_factor;
    nivometro->scale.offset = cal_data->hx711_offset;
    
    // Aplicar calibración HC-SR04P
    hcsr04p_set_calibration(&nivometro->ultrasonic, cal_data->hcsr04p_cal_factor);
    
    ESP_LOGI(TAG, "Calibraciones aplicadas a los sensores");
    return ESP_OK;
}


void boot_button_init(void) {
    // Configurar botón BOOT
    gpio_config_t boot_btn_config = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&boot_btn_config);
    
    ESP_LOGI(TAG, "Botón BOOT inicializado en GPIO %d", BOOT_BUTTON_PIN);
}

bool boot_button_check_calibration_mode(void) {
    ESP_LOGI(TAG, "INSTRUCCIONES: Mantén presionado el botón BOOT para calibrar");
    
    uint32_t pressed_time = 0;
    const uint32_t check_interval = 100; // Verificar cada 100ms
    const uint32_t required_time = CONFIG_CALIBRATION_BOOT_HOLD_TIME_MS;
    
    // Verificar durante 5 segundos si el botón está presionado
    for (int i = 0; i < 50; i++) {  // 50 * 100ms = 5 segundos
        if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {  // Botón presionado (activo bajo)
            pressed_time += check_interval;
            ESP_LOGI(TAG, "Botón BOOT presionado - Tiempo: %lu ms / %lu ms", 
                    pressed_time, required_time);
            
            if (pressed_time >= required_time) {
                ESP_LOGI(TAG, "Modo calibración activado!");
                return true;
            }
        } else {
            if (pressed_time > 0) {
                ESP_LOGI(TAG, "Botón liberado - Tiempo insuficiente: %lu ms", pressed_time);
            }
            pressed_time = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(check_interval));
    }
    
    ESP_LOGI(TAG, "Continuando con arranque normal");
    return false;
}

void boot_button_wait_for_press(void) {
    ESP_LOGI(TAG, "Esperando confirmación (presiona BOOT)...");
    
    // Esperar a que el botón se libere (si está presionado)
    while (gpio_get_level(BOOT_BUTTON_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Esperar a que se presione
    while (gpio_get_level(BOOT_BUTTON_PIN) == 1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Anti-rebote
    vTaskDelay(pdMS_TO_TICKS(CALIBRATION_DEBOUNCE_MS));
    
    // Esperar a que se libere
    while (gpio_get_level(BOOT_BUTTON_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Confirmación recibida");
}

bool validate_calibration_data(const calibration_data_t *cal_data) {
    if (!cal_data) return false;
    
    // Verificar magic number
    if (cal_data->magic_number != CALIBRATION_MAGIC_NUMBER) {
        return false;
    }
    
    // Verificar flag de calibración
    if (!cal_data->calibrated) {
        return false;
    }
    
    // Verificar que los valores estén en rangos razonables
    if (cal_data->hx711_scale_factor <= 0.0f || cal_data->hx711_scale_factor > 1000000.0f) {
        return false;
    }
    
    if (cal_data->hcsr04p_cal_factor <= 0.0f || cal_data->hcsr04p_cal_factor > 10.0f) {
        return false;
    }
    
    return true;
}

float calculate_error_percent(float measured, float expected) {
    if (expected == 0.0f) return 100.0f;
    return fabsf((measured - expected) / expected) * 100.0f;
}

bool is_value_in_tolerance(float measured, float expected, float tolerance_percent) {
    float error = calculate_error_percent(measured, expected);
    return error <= tolerance_percent;
}

uint32_t get_timestamp_seconds(void) {
    return (uint32_t)(esp_timer_get_time() / 1000000);
}

void format_calibration_summary(const calibration_data_t *cal_data, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size,
        "Calibración: HX711[scale=%.6f, offset=%ld] HC-SR04P[factor=%.6f] Peso=%.1fg Dist=%.1fcm",
        cal_data->hx711_scale_factor,
        cal_data->hx711_offset,
        cal_data->hcsr04p_cal_factor,
        cal_data->known_weight_used,
        cal_data->known_distance_used);
}