// File: components/nivometro_sensors/include/hx711.h

#ifndef HX711_H
#define HX711_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Definiciones de ganancia
typedef enum {
    HX711_GAIN_128 = 1,  // Canal A, ganancia 128 (1 pulso extra)
    HX711_GAIN_32  = 2,  // Canal B, ganancia 32 (2 pulsos extra)
    HX711_GAIN_64  = 3   // Canal A, ganancia 64 (3 pulsos extra)
} hx711_gain_t;

// Estructura de configuración
typedef struct {
    gpio_num_t dout_pin;
    gpio_num_t sck_pin;
    hx711_gain_t gain;
} hx711_config_t;

// Estructura del dispositivo HX711
typedef struct {
    gpio_num_t dout_pin;
    gpio_num_t sck_pin;
    hx711_gain_t gain;
    int32_t offset;
    float scale;
    bool is_ready;
    int32_t last_readings[5];
    int reading_index;
} hx711_t;

typedef hx711_t hx711_sensor_t;

// Constantes
#define HX711_TIMEOUT_MS        1000
#define HX711_STABILIZE_TIME_MS 100
#define HX711_READ_TIMEOUT_MS   500
#define HX711_POWER_UP_TIME_MS  100

// Funciones principales (NUEVA API)
esp_err_t hx711_init(hx711_t *dev, const hx711_config_t *config);
esp_err_t hx711_deinit(hx711_t *dev);
bool hx711_is_ready(hx711_t *dev);
esp_err_t hx711_power_up(hx711_t *dev);
esp_err_t hx711_power_down(hx711_t *dev);
esp_err_t hx711_set_gain(hx711_t *dev, hx711_gain_t gain);
esp_err_t hx711_read_raw(hx711_t *dev, int32_t *raw_value);
esp_err_t hx711_read_average(hx711_t *dev, int32_t *avg_value, int samples);
esp_err_t hx711_tare(hx711_t *dev, int samples);
esp_err_t hx711_calibrate(hx711_t *dev, float known_weight, int samples);
esp_err_t hx711_read_units(hx711_t *dev, float *units);
esp_err_t hx711_read_units_average(hx711_t *dev, float *units, int samples);
void hx711_debug_info(hx711_t *dev);
static inline bool hx711_init_old_api(hx711_sensor_t *sensor, int dout_pin, int sck_pin, hx711_gain_t gain) {
    hx711_config_t config = {
        .dout_pin = (gpio_num_t)dout_pin,
        .sck_pin = (gpio_num_t)sck_pin,
        .gain = gain
    };
    return (hx711_init(sensor, &config) == ESP_OK);
}
static inline int32_t hx711_read_raw_old_api(hx711_sensor_t *sensor) {
    int32_t raw_value;
    if (hx711_read_raw(sensor, &raw_value) == ESP_OK) {
        return raw_value;
    }
    return 0;
}
static inline float hx711_read_units_old_api(hx711_sensor_t *sensor) {
    float units;
    if (hx711_read_units(sensor, &units) == ESP_OK) {
        return units;
    }
    return 0.0f;
}
static inline void hx711_calibrate_old_api(hx711_sensor_t *sensor, float known_weight, int readings) {
    hx711_calibrate(sensor, known_weight, readings);
}
static inline void hx711_tare_old_api(hx711_sensor_t *sensor, int readings) {
    hx711_tare(sensor, readings);
}
static inline void hx711_set_gain_old_api(hx711_sensor_t *sensor, hx711_gain_t gain) {
    hx711_set_gain(sensor, gain);
}
static inline void hx711_power_down_old_api(hx711_sensor_t *sensor) {
    hx711_power_down(sensor);
}
static inline void hx711_power_up_old_api(hx711_sensor_t *sensor) {
    hx711_power_up(sensor);
}
#ifdef __cplusplus
}
#endif
#endif 