// hcsr04p.h
#ifndef HCSR04P_H
#define HCSR04P_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"

typedef struct {
    int trigger_pin;
    int echo_pin;
    float distance_cm;
    float calibration_factor;
} hcsr04p_sensor_t;

/**
 * @brief Inicializa el sensor HC-SR04P
 * @param sensor Puntero a la estructura del sensor
 * @param trigger_pin Pin de trigger
 * @param echo_pin Pin de echo
 * @return true si la inicialización fue exitosa
 */
bool hcsr04p_init(hcsr04p_sensor_t *sensor, int trigger_pin, int echo_pin);

/**
 * @brief Lee la distancia del sensor
 * @param sensor Puntero a la estructura del sensor
 * @return Distancia en centímetros o -1 si hay error
 */
float hcsr04p_read_distance(hcsr04p_sensor_t *sensor);

/**
 * @brief Configura el factor de calibración
 * @param sensor Puntero a la estructura del sensor
 * @param factor Factor de calibración
 */
void hcsr04p_set_calibration(hcsr04p_sensor_t *sensor, float factor);

#endif // HCSR04P_H