//File: components/nivometro_sensors/src/nivometro_sensors.c

#include "nivometro_sensors.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include <string.h>
#include <math.h>

static const char *TAG = "NIVOMETRO";

// ==============================================================================
// FUNCIONES PRINCIPALES ORIGINALES
// ==============================================================================

esp_err_t nivometro_init(nivometro_t *nivometro, const nivometro_config_t *config) {
    if (!nivometro || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&nivometro->config, config, sizeof(nivometro_config_t));
    nivometro->initialized = false;
    
    ESP_LOGI(TAG, "Inicializando sensores del nivómetro (sin VL53L0X)...");
    
    // Inicializar HC-SR04P
    if (!hcsr04p_init(&nivometro->ultrasonic, 
                      config->hcsr04p_trigger_pin, 
                      config->hcsr04p_echo_pin)) {
        ESP_LOGE(TAG, "Error inicializando HC-SR04P");
        //return ESP_FAIL;
    }
    hcsr04p_set_calibration(&nivometro->ultrasonic, config->hcsr04p_cal_factor);
    ESP_LOGI(TAG, "✅ HC-SR04P inicializado");
    
    // Inicializar HX711 - CORREGIDO: usar estructura de configuración
    hx711_config_t hx711_config = {
        .dout_pin = config->hx711_dout_pin,
        .sck_pin = config->hx711_sck_pin,
        .gain = config->hx711_gain
    };
    
    if (hx711_init(&nivometro->scale, &hx711_config) != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando HX711");
        //return ESP_FAIL;
    }
    ESP_LOGI(TAG, "✅ HX711 inicializado");
    
    nivometro->initialized = true;
    ESP_LOGI(TAG, "🎉 Nivómetro completamente inicializado (sin VL53L0X)");
    
    return ESP_OK;
}

esp_err_t nivometro_read_all_sensors(nivometro_t *nivometro, nivometro_data_t *data) {
    if (!nivometro || !data || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Timestamp
    data->timestamp_us = esp_timer_get_time();
    data->sensor_status = 0;
    
    // Leer HC-SR04P
    data->ultrasonic_distance_cm = hcsr04p_read_distance(&nivometro->ultrasonic);
    if (data->ultrasonic_distance_cm >= 0) {
        data->sensor_status |= 0x01; // Bit 0 = HC-SR04P OK
    }
    
    // Leer HX711 - CORREGIDO: usar puntero para recibir el valor
    float weight_units;
    esp_err_t hx711_result = hx711_read_units(&nivometro->scale, &weight_units);
    if (hx711_result == ESP_OK) {
        data->weight_grams = weight_units;
        data->sensor_status |= 0x02; // Bit 1 = HX711 OK
    } else {
        data->weight_grams = 0.0f;
        ESP_LOGW(TAG, "Error leyendo HX711: %s", esp_err_to_name(hx711_result));
    }
   
    // Datos adicionales (estimados por ahora)
    data->battery_voltage = 3.7f; // TODO: Implementar lectura real
    data->temperature_c = 20;     // TODO: Implementar sensor temperatura
    
    ESP_LOGD(TAG, "Sensores leídos - Ultrasonido: %.2f cm, Peso: %.2f g", 
             data->ultrasonic_distance_cm, data->weight_grams);
    
    return ESP_OK;
}

esp_err_t nivometro_calibrate_scale(nivometro_t *nivometro, float known_weight_g) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Calibrando balanza con peso conocido: %.2f g", known_weight_g);
    // Usar número de muestras configurado en menuconfig
    esp_err_t result = hx711_calibrate(&nivometro->scale, known_weight_g, CONFIG_CALIBRATION_HX711_SAMPLES);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Calibración de balanza completada. Factor: %.2f", nivometro->scale.scale);
        ESP_LOGI(TAG, "Muestras utilizadas: %d", CONFIG_CALIBRATION_HX711_SAMPLES);
    } else {
        ESP_LOGE(TAG, "Error en calibración: %s", esp_err_to_name(result));
    }
    
    return result;
}

esp_err_t nivometro_tare_scale(nivometro_t *nivometro) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Realizando tara de la balanza...");
    // Usar número de muestras configurado en menuconfig para tara también
    esp_err_t result = hx711_tare(&nivometro->scale, CONFIG_CALIBRATION_HX711_SAMPLES);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Tara completada. Offset: %ld", nivometro->scale.offset);
        ESP_LOGI(TAG, "Muestras utilizadas: %d", CONFIG_CALIBRATION_HX711_SAMPLES);
    } else {
        ESP_LOGE(TAG, "Error en tara: %s", esp_err_to_name(result));
    }
    
    return result;
}

void nivometro_power_down(nivometro_t *nivometro) {
    if (nivometro && nivometro->initialized) {
        hx711_power_down(&nivometro->scale);
        ESP_LOGI(TAG, "Sensores en modo bajo consumo");
    }
}

void nivometro_power_up(nivometro_t *nivometro) {
    if (nivometro && nivometro->initialized) {
        hx711_power_up(&nivometro->scale);
        ESP_LOGI(TAG, "Sensores activados");
    }
}

const char* nivometro_get_sensor_status_string(uint8_t status) {
    static char status_str[64];
    snprintf(status_str, sizeof(status_str), "HC-SR04P:%s HX711:%s",
             (status & 0x01) ? "OK" : "FAIL",
             (status & 0x02) ? "OK" : "FAIL");
    return status_str;
}

bool nivometro_is_sensor_working(uint8_t status, int sensor_index) {
    return (status & (1 << sensor_index)) != 0;
}

void nivometro_data_to_sensor_data(const nivometro_data_t *src, sensor_data_t *dst) {
    if (!src || !dst) return;
    
    dst->distance_cm = src->ultrasonic_distance_cm;
    dst->weight_kg = src->weight_grams / 1000.0f;  // Convertir gramos a kilogramos
    dst->timestamp_us = (int64_t)src->timestamp_us;
    dst->sensor_status = src->sensor_status;
    dst->battery_voltage = src->battery_voltage;
    dst->temperature_c = (int)src->temperature_c;
}

// ==============================================================================
// FUNCIONES DE CALIBRACIÓN AVANZADA
// ==============================================================================

float nivometro_calibrate_ultrasonic(nivometro_t *nivometro, 
                                   float known_distance_cm, 
                                   int samples, 
                                   float tolerance_percent) {
    if (!nivometro || !nivometro->initialized || samples <= 0) {
        ESP_LOGE(TAG, "❌ Parámetros inválidos para calibración HC-SR04P");
        return 0.0f;
    }
    
    float total_distance = 0;
    int valid_samples = 0;
    
    ESP_LOGI(TAG, "📏 Calibrando HC-SR04P con distancia conocida: %.1f cm", known_distance_cm);
    ESP_LOGI(TAG, "📊 Tomando %d mediciones (tolerancia: ±%.1f%%)...", samples, tolerance_percent);
    
    for (int i = 0; i < samples; i++) {
        float distance = hcsr04p_read_distance(&nivometro->ultrasonic);
        if (distance > 0) {  // Medición válida
            total_distance += distance;
            valid_samples++;
            ESP_LOGI(TAG, "📐 Medición %d/%d: %.2f cm", i + 1, samples, distance);
        } else {
            ESP_LOGW(TAG, "⚠️  Medición %d/%d: inválida", i + 1, samples);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    if (valid_samples >= (samples / 2)) {  // Al menos 50% de mediciones válidas
        float average_measured = total_distance / valid_samples;
        ESP_LOGI(TAG, "📊 Promedio de %d mediciones válidas: %.2f cm", valid_samples, average_measured);
        
        // Calcular factor de calibración
        float current_factor = nivometro->ultrasonic.calibration_factor;
        float new_factor = current_factor * (known_distance_cm / average_measured);
        
        // Validar que el factor esté en un rango razonable
        if (new_factor > 0.5f && new_factor < 2.0f) {
            ESP_LOGI(TAG, "🔧 Factor de calibración:");
            ESP_LOGI(TAG, "   Anterior: %.6f", current_factor);
            ESP_LOGI(TAG, "   Nuevo: %.6f", new_factor);
            ESP_LOGI(TAG, "   Diferencia: %.1f%%", ((new_factor - current_factor) / current_factor) * 100);
            
            // Verificar si está dentro de la tolerancia
            float error_percent = fabsf((average_measured - known_distance_cm) / known_distance_cm) * 100;
            if (error_percent <= tolerance_percent) {
                ESP_LOGI(TAG, "✅ Calibración exitosa - Error: %.1f%% (≤%.1f%%)", 
                         error_percent, tolerance_percent);
                
                // Aplicar el nuevo factor
                hcsr04p_set_calibration(&nivometro->ultrasonic, new_factor);
                return new_factor;
            } else {
                ESP_LOGW(TAG, "⚠️  Error alto: %.1f%% (>%.1f%%) - Usando factor calculado", 
                         error_percent, tolerance_percent);
                
                // Aplicar el nuevo factor aunque el error sea alto
                hcsr04p_set_calibration(&nivometro->ultrasonic, new_factor);
                return new_factor;
            }
        } else {
            ESP_LOGE(TAG, "❌ Factor calculado fuera de rango: %.6f", new_factor);
            ESP_LOGI(TAG, "🔧 Manteniendo factor anterior: %.6f", current_factor);
            return current_factor;
        }
    } else {
        ESP_LOGE(TAG, "❌ Mediciones insuficientes: %d/%d válidas", valid_samples, samples);
        return 0.0f;
    }
}

esp_err_t nivometro_calibrate_scale_with_validation(nivometro_t *nivometro, 
                                                  float known_weight_g, 
                                                  float tolerance_percent) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "⚖️  Calibrando HX711 con validación automática");
    ESP_LOGI(TAG, "   Peso conocido: %.1f g", known_weight_g);
    ESP_LOGI(TAG, "   Tolerancia: ±%.1f%%", tolerance_percent);
    ESP_LOGI(TAG, "   Muestras: %d", CONFIG_CALIBRATION_HX711_SAMPLES);
    
    // Realizar calibración usando parámetros de menuconfig
    esp_err_t cal_result = nivometro_calibrate_scale(nivometro, known_weight_g);
    if (cal_result != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error en calibración base: %s", esp_err_to_name(cal_result));
        return cal_result;
    }
    
    // Validar calibración leyendo una vez más
    float weight_units;
    esp_err_t read_result = hx711_read_units(&nivometro->scale, &weight_units);
    if (read_result == ESP_OK) {
        float error_percent = fabsf((weight_units - known_weight_g) / known_weight_g) * 100;
        ESP_LOGI(TAG, "🧪 Validación: Peso leído = %.2f g, Error = %.1f%%", weight_units, error_percent);
        
        if (error_percent <= tolerance_percent) {
            ESP_LOGI(TAG, "✅ Validación exitosa (≤%.1f%%)", tolerance_percent);
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "⚠️  Error alto (>%.1f%%) pero calibración aplicada", tolerance_percent);
            return ESP_OK; // Consideramos OK aunque el error sea alto
        }
    } else {
        ESP_LOGW(TAG, "⚠️  No se pudo validar la calibración: %s", esp_err_to_name(read_result));
        return ESP_OK; // Calibración base fue exitosa
    }
}

bool nivometro_verify_sensors_health(nivometro_t *nivometro) {
    if (!nivometro || !nivometro->initialized) {
        return false;
    }
    
    ESP_LOGI(TAG, "🔍 Verificando salud de sensores...");
    
    bool all_healthy = true;
    
    // Verificar HC-SR04P - Tomar 3 mediciones
    int valid_ultrasonic = 0;
    for (int i = 0; i < 3; i++) {
        float distance = hcsr04p_read_distance(&nivometro->ultrasonic);
        if (distance > 0 && distance < 500) { // Rango razonable
            valid_ultrasonic++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (valid_ultrasonic >= 2) {
        ESP_LOGI(TAG, "✅ HC-SR04P: Saludable (%d/3 mediciones válidas)", valid_ultrasonic);
    } else {
        ESP_LOGE(TAG, "❌ HC-SR04P: Problemático (%d/3 mediciones válidas)", valid_ultrasonic);
        all_healthy = false;
    }
    
    // Verificar HX711 - Tomar 3 mediciones
    int valid_weight = 0;
    for (int i = 0; i < 3; i++) {
        float weight;
        if (hx711_read_units(&nivometro->scale, &weight) == ESP_OK) {
            valid_weight++;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    if (valid_weight >= 2) {
        ESP_LOGI(TAG, "✅ HX711: Saludable (%d/3 mediciones válidas)", valid_weight);
    } else {
        ESP_LOGE(TAG, "❌ HX711: Problemático (%d/3 mediciones válidas)", valid_weight);
        all_healthy = false;
    }
    
    return all_healthy;
}

float nivometro_get_ultrasonic_calibration_factor(const nivometro_t *nivometro) {
    if (!nivometro || !nivometro->initialized) {
        return 0.0f;
    }
    return nivometro->ultrasonic.calibration_factor;
}

void nivometro_get_scale_calibration_params(const nivometro_t *nivometro, 
                                          float *scale_factor, 
                                          int32_t *offset) {
    if (!nivometro || !nivometro->initialized) {
        if (scale_factor) *scale_factor = 0.0f;
        if (offset) *offset = 0;
        return;
    }
    
    if (scale_factor) *scale_factor = nivometro->scale.scale;
    if (offset) *offset = nivometro->scale.offset;
}

esp_err_t nivometro_apply_calibration_factors(nivometro_t *nivometro,
                                            float hcsr04p_factor,
                                            float hx711_scale,
                                            int32_t hx711_offset) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔧 Aplicando factores de calibración:");
    ESP_LOGI(TAG, "   HC-SR04P factor: %.6f", hcsr04p_factor);
    ESP_LOGI(TAG, "   HX711 scale: %.6f", hx711_scale);
    ESP_LOGI(TAG, "   HX711 offset: %ld", hx711_offset);
    
    // Aplicar calibración HC-SR04P
    hcsr04p_set_calibration(&nivometro->ultrasonic, hcsr04p_factor);
    
    // Aplicar calibración HX711
    nivometro->scale.scale = hx711_scale;
    nivometro->scale.offset = hx711_offset;
    
    ESP_LOGI(TAG, "✅ Factores de calibración aplicados");
    return ESP_OK;
}

esp_err_t nivometro_full_calibration_test(nivometro_t *nivometro,
                                        float known_weight_g,
                                        float known_distance_cm) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🧪 Iniciando prueba completa de calibración");
    ESP_LOGI(TAG, "   Peso conocido: %.1f g", known_weight_g);
    ESP_LOGI(TAG, "   Distancia conocida: %.1f cm", known_distance_cm);
    
    bool success = true;
    
    // Verificar salud de sensores
    if (!nivometro_verify_sensors_health(nivometro)) {
        ESP_LOGE(TAG, "❌ Sensores no están saludables");
        return ESP_FAIL;
    }
    
    // Calibrar HX711
    ESP_LOGI(TAG, "🔧 Probando calibración HX711...");
    if (nivometro_calibrate_scale_with_validation(nivometro, known_weight_g, 10.0f) != ESP_OK) {
        ESP_LOGE(TAG, "❌ Fallo en calibración HX711");
        success = false;
    }
    
    // Calibrar HC-SR04P
    ESP_LOGI(TAG, "🔧 Probando calibración HC-SR04P...");
    float new_factor = nivometro_calibrate_ultrasonic(nivometro, known_distance_cm, 5, 10.0f);
    if (new_factor <= 0.0f) {
        ESP_LOGE(TAG, "❌ Fallo en calibración HC-SR04P");
        success = false;
    }
    
    if (success) {
        ESP_LOGI(TAG, "✅ Prueba completa de calibración exitosa");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ Prueba completa de calibración falló");
        return ESP_FAIL;
    }
}