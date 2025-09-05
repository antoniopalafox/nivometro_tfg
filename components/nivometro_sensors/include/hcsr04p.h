// File: components/nivometro_sensors/include/hcsr04p.h

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

bool hcsr04p_init(hcsr04p_sensor_t *sensor, int trigger_pin, int echo_pin); //Inicializa el sensor HC-SR04P

float hcsr04p_read_distance(hcsr04p_sensor_t *sensor); //Lee la distancia del sensor en centímetros

void hcsr04p_set_calibration(hcsr04p_sensor_t *sensor, float factor); //Configura el factor de calibración

#endif