#include "nivometro_sensors.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "NIVOMETRO";

esp_err_t nivometro_init(nivometro_t *nivometro, const nivometro_config_t *config) {
    if (!nivometro || !config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&nivometro->config, config, sizeof(nivometro_config_t));
    nivometro->initialized = false;
    
    ESP_LOGI(TAG, "Inicializando sensores del nivómetro...");
    
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
    
    // Inicializar VL53L0X
    if (!vl53l0x_init(&nivometro->laser,
                      config->vl53l0x_i2c_port,
                      config->vl53l0x_address)) {
        ESP_LOGE(TAG, "Error inicializando VL53L0X");
        //return ESP_FAIL;
    }
    vl53l0x_set_accuracy(&nivometro->laser, config->vl53l0x_accuracy);
    vl53l0x_set_calibration(&nivometro->laser, config->vl53l0x_cal_factor);
    ESP_LOGI(TAG, "✅ VL53L0X inicializado");
    
    nivometro->initialized = true;
    ESP_LOGI(TAG, "🎉 Nivómetro completamente inicializado");
    
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
    
    // Leer VL53L0X
    uint16_t laser_mm = vl53l0x_read_distance(&nivometro->laser);
    data->laser_distance_mm = (float)laser_mm;
    if (laser_mm > 0 && laser_mm < 8000) { // Rango válido del VL53L0X
        data->sensor_status |= 0x04; // Bit 2 = VL53L0X OK
    }
    
    // Datos adicionales (estimados por ahora)
    data->battery_voltage = 3.7f; // TODO: Implementar lectura real
    data->temperature_c = 20;     // TODO: Implementar sensor temperatura
    
    ESP_LOGD(TAG, "Sensores leídos - Ultrasonido: %.2f cm, Peso: %.2f g, Láser: %.0f mm", 
             data->ultrasonic_distance_cm, data->weight_grams, data->laser_distance_mm);
    
    return ESP_OK;
}

esp_err_t nivometro_calibrate_scale(nivometro_t *nivometro, float known_weight_g) {
    if (!nivometro || !nivometro->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Calibrando balanza con peso conocido: %.2f g", known_weight_g);
    esp_err_t result = hx711_calibrate(&nivometro->scale, known_weight_g, 10);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Calibración de balanza completada. Factor: %.2f", nivometro->scale.scale);
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
    esp_err_t result = hx711_tare(&nivometro->scale, 10);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Tara completada. Offset: %ld", nivometro->scale.offset);
    } else {
        ESP_LOGE(TAG, "Error en tara: %s", esp_err_to_name(result));
    }
    
    return result;
}

void nivometro_power_down(nivometro_t *nivometro) {
    if (nivometro && nivometro->initialized) {
        hx711_power_down(&nivometro->scale);
        vl53l0x_sleep(&nivometro->laser);
        ESP_LOGI(TAG, "Sensores en modo bajo consumo");
    }
}

void nivometro_power_up(nivometro_t *nivometro) {
    if (nivometro && nivometro->initialized) {
        hx711_power_up(&nivometro->scale);
        vl53l0x_wake_up(&nivometro->laser);
        ESP_LOGI(TAG, "Sensores activados");
    }
}

const char* nivometro_get_sensor_status_string(uint8_t status) {
    static char status_str[64];
    snprintf(status_str, sizeof(status_str), "HC-SR04P:%s HX711:%s VL53L0X:%s",
             (status & 0x01) ? "OK" : "FAIL",
             (status & 0x02) ? "OK" : "FAIL", 
             (status & 0x04) ? "OK" : "FAIL");
    return status_str;
}

bool nivometro_is_sensor_working(uint8_t status, int sensor_index) {
    return (status & (1 << sensor_index)) != 0;
}

void nivometro_data_to_sensor_data(const nivometro_data_t *src, sensor_data_t *dst) {
    if (!src || !dst) return;
    
    dst->distance_cm = src->ultrasonic_distance_cm;
    dst->weight_kg = src->weight_grams / 1000.0f;  // Convertir gramos a kilogramos
    dst->laser_mm = src->laser_distance_mm;
    dst->timestamp_us = (int64_t)src->timestamp_us;
    dst->sensor_status = src->sensor_status;
    dst->battery_voltage = src->battery_voltage;
    dst->temperature_c = (int)src->temperature_c;
}