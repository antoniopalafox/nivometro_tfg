// File: components//nivometro_sensors/src/hx711.c 

#include "hx711.h"
#include "esp_rom_sys.h"  

static const char *TAG = "HX711";

// Función privada para leer un bit
static inline bool hx711_read_bit(hx711_t *dev)
{
    gpio_set_level(dev->sck_pin, 1);
    esp_rom_delay_us(1);  // Delay más corto pero suficiente
    bool bit = gpio_get_level(dev->dout_pin);
    gpio_set_level(dev->sck_pin, 0);
    esp_rom_delay_us(1);
    return bit;
}

// Función privada para esperar a que el sensor esté listo
static esp_err_t hx711_wait_ready(hx711_t *dev, int timeout_ms)
{
    int timeout_us = timeout_ms * 1000;
    int elapsed_us = 0;
    
    while (gpio_get_level(dev->dout_pin) == 1) {
        if (elapsed_us >= timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(100);
        elapsed_us += 100;
    }
    
    return ESP_OK;
}

esp_err_t hx711_init(hx711_t *dev, const hx711_config_t *config)
{
    if (!dev || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    // Inicializar estructura
    dev->dout_pin = config->dout_pin;
    dev->sck_pin = config->sck_pin;
    dev->gain = config->gain;
    dev->offset = 0;
    dev->scale = 1.0f;
    dev->is_ready = false;
    dev->reading_index = 0;
    
    // Limpiar historial de lecturas
    memset(dev->last_readings, 0, sizeof(dev->last_readings));

    // Configurar pin SCK como salida
    gpio_config_t sck_config = {
        .pin_bit_mask = (1ULL << dev->sck_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&sck_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando pin SCK: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configurar pin DOUT como entrada
    gpio_config_t dout_config = {
        .pin_bit_mask = (1ULL << dev->dout_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    ret = gpio_config(&dout_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando pin DOUT: %s", esp_err_to_name(ret));
        return ret;
    }

    // Inicializar SCK en LOW
    gpio_set_level(dev->sck_pin, 0);
    
    // Esperar estabilización
    vTaskDelay(pdMS_TO_TICKS(HX711_STABILIZE_TIME_MS));
    
    // Despertar el sensor
    ret = hx711_power_up(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error despertando sensor");
        return ret;
    }

    // Configurar ganancia
    ret = hx711_set_gain(dev, dev->gain);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurando ganancia");
        return ret;
    }

    // Verificar que el sensor esté funcionando
    vTaskDelay(pdMS_TO_TICKS(200));
    
    if (hx711_is_ready(dev)) {
        dev->is_ready = true;
        ESP_LOGI(TAG, "HX711 inicializado correctamente");
    } else {
        ESP_LOGW(TAG, "HX711 inicializado pero el sensor no está listo");
    }

    return ESP_OK;
}

esp_err_t hx711_deinit(hx711_t *dev)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    // Poner el sensor en modo sleep
    hx711_power_down(dev);
    
    // Resetear configuración de GPIO
    gpio_reset_pin(dev->dout_pin);
    gpio_reset_pin(dev->sck_pin);
    
    dev->is_ready = false;
    
    return ESP_OK;
}

bool hx711_is_ready(hx711_t *dev)
{
    if (!dev) {
        return false;
    }
    
    return (gpio_get_level(dev->dout_pin) == 0);
}

esp_err_t hx711_power_up(hx711_t *dev)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(dev->sck_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(HX711_POWER_UP_TIME_MS));
    
    ESP_LOGI(TAG, "Sensor despertado");
    return ESP_OK;
}

esp_err_t hx711_power_down(hx711_t *dev)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_set_level(dev->sck_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Sensor en modo sleep");
    return ESP_OK;
}

esp_err_t hx711_set_gain(hx711_t *dev, hx711_gain_t gain)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    dev->gain = gain;
    
    // Para cambiar la ganancia, necesitamos hacer una lectura
    // y luego enviar los pulsos adicionales correspondientes
    int32_t dummy_value;
    esp_err_t ret = hx711_read_raw(dev, &dummy_value);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Ganancia configurada a: %d", (int)gain);
    }
    
    return ret;
}

esp_err_t hx711_read_raw(hx711_t *dev, int32_t *raw_value)
{
    if (!dev || !raw_value) {
        return ESP_ERR_INVALID_ARG;
    }

    // Esperar a que el sensor esté listo con timeout más largo
    esp_err_t ret = hx711_wait_ready(dev, HX711_READ_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Sensor no está listo para lectura");
        *raw_value = INT32_MIN;  // Valor que indica error
        return ESP_ERR_TIMEOUT;
    }

    // Leer 24 bits de datos
    uint32_t value = 0;
    
    // Deshabilitar interrupciones durante la lectura crítica
    portDISABLE_INTERRUPTS();
    
    for (int i = 0; i < 24; i++) {
        value <<= 1;
        if (hx711_read_bit(dev)) {
            value |= 1;
        }
    }
    
    // Pulsos adicionales para configurar ganancia
    for (int i = 0; i < (int)dev->gain; i++) {
        gpio_set_level(dev->sck_pin, 1);
        esp_rom_delay_us(1);
        gpio_set_level(dev->sck_pin, 0);
        esp_rom_delay_us(1);
    }
    
    portENABLE_INTERRUPTS();

    // Convertir a valor con signo (complemento a 2)
    if (value & 0x800000) {
        value |= 0xFF000000;
    }
    
    *raw_value = (int32_t)value;
    
    // Guardar en historial
    dev->last_readings[dev->reading_index] = *raw_value;
    dev->reading_index = (dev->reading_index + 1) % 5;
    
    return ESP_OK;
}

esp_err_t hx711_read_average(hx711_t *dev, int32_t *avg_value, int samples)
{
    if (!dev || !avg_value || samples <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t sum = 0;
    int valid_samples = 0;
    
    for (int i = 0; i < samples; i++) {
        int32_t raw_value;
        esp_err_t ret = hx711_read_raw(dev, &raw_value);
        
        if (ret == ESP_OK && raw_value != INT32_MIN) {
            sum += raw_value;
            valid_samples++;
        }
        
        // Pequeño delay entre lecturas
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (valid_samples == 0) {
        ESP_LOGE(TAG, "No se pudieron obtener lecturas válidas");
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    *avg_value = (int32_t)(sum / valid_samples);
    
    if (valid_samples < samples) {
        ESP_LOGW(TAG, "Solo se obtuvieron %d de %d lecturas válidas", valid_samples, samples);
    }
    
    return ESP_OK;
}

esp_err_t hx711_tare(hx711_t *dev, int samples)
{
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Realizando tara con %d muestras...", samples);
    
    int32_t avg_value;
    esp_err_t ret = hx711_read_average(dev, &avg_value, samples);
    
    if (ret == ESP_OK) {
        dev->offset = avg_value;
        ESP_LOGI(TAG, "Tara completada. Offset: %ld", (long)dev->offset);
    } else {
        ESP_LOGE(TAG, "Error durante la tara");
    }
    
    return ret;
}

esp_err_t hx711_calibrate(hx711_t *dev, float known_weight, int samples)
{
    if (!dev || known_weight <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Calibrando con peso conocido: %.2f g", known_weight);
    
    int32_t avg_value;
    esp_err_t ret = hx711_read_average(dev, &avg_value, samples);
    
    if (ret == ESP_OK) {
        float net_value = (float)(avg_value - dev->offset);
        if (net_value != 0) {
            dev->scale = net_value / known_weight;
            ESP_LOGI(TAG, "Calibración completada. Escala: %.2f", dev->scale);
        } else {
            ESP_LOGE(TAG, "Error en calibración: valor neto es cero");
            return ESP_ERR_INVALID_RESPONSE;
        }
    } else {
        ESP_LOGE(TAG, "Error durante la calibración");
    }
    
    return ret;
}

esp_err_t hx711_read_units(hx711_t *dev, float *units)
{
    if (!dev || !units) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t raw_value;
    esp_err_t ret = hx711_read_raw(dev, &raw_value);
    
    if (ret == ESP_OK && raw_value != INT32_MIN) {
        *units = (float)(raw_value - dev->offset) / dev->scale;
    } else {
        *units = 0.0f;
    }
    
    return ret;
}

esp_err_t hx711_read_units_average(hx711_t *dev, float *units, int samples)
{
    if (!dev || !units) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t avg_raw;
    esp_err_t ret = hx711_read_average(dev, &avg_raw, samples);
    
    if (ret == ESP_OK) {
        *units = (float)(avg_raw - dev->offset) / dev->scale;
    } else {
        *units = 0.0f;
    }
    
    return ret;
}

void hx711_debug_info(hx711_t *dev)
{
    if (!dev) {
        return;
    }

    ESP_LOGI(TAG, "=== HX711 DEBUG INFO ===");
    ESP_LOGI(TAG, "DOUT Pin: %d", dev->dout_pin);
    ESP_LOGI(TAG, "SCK Pin: %d", dev->sck_pin);
    ESP_LOGI(TAG, "Offset: %ld", (long)dev->offset);
    ESP_LOGI(TAG, "Scale: %f", dev->scale);
    ESP_LOGI(TAG, "Gain: %d", dev->gain);
    ESP_LOGI(TAG, "Sensor Ready: %s", hx711_is_ready(dev) ? "YES" : "NO");
    ESP_LOGI(TAG, "Lecturas RAW recientes:");
    
    for (int i = 0; i < 5; i++) {
        int32_t raw_value;
        if (hx711_read_raw(dev, &raw_value) == ESP_OK) {
            ESP_LOGI(TAG, "  Lectura %d: %ld", i + 1, (long)raw_value);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}