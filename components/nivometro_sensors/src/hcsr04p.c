// File: components/nivometro_sensors/src/hcsr04p.c
#include "hcsr04p.h"

#define HCSR04P_TIMEOUT_US 25000  // 25ms timeout
#define SOUND_SPEED_CM_US 0.0343  // Velocidad del sonido en cm/us

bool hcsr04p_init(hcsr04p_sensor_t *sensor, int trigger_pin, int echo_pin) {
    if (sensor == NULL) {
        return false;
    }
    
    sensor->trigger_pin = trigger_pin;
    sensor->echo_pin = echo_pin;
    sensor->distance_cm = 0.0f;
    sensor->calibration_factor = 1.0f;

    // Configurar pines
    gpio_config_t io_conf = {};
    
    // Configurar trigger pin como salida
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << trigger_pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Configurar echo pin como entrada
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << echo_pin);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Inicializar el trigger en nivel bajo
    gpio_set_level(trigger_pin, 0);
    
    return true;
}

float hcsr04p_read_distance(hcsr04p_sensor_t *sensor) {
    if (sensor == NULL) {
        return -1;
    }
    
    int64_t echo_start, echo_end;
    float distance;
    
    // Enviar pulso de trigger (10us)
    gpio_set_level(sensor->trigger_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(sensor->trigger_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(sensor->trigger_pin, 0);
    
    // Esperar a que el pin echo se active (nivel alto)
    int64_t start_time = esp_timer_get_time();
    while (gpio_get_level(sensor->echo_pin) == 0) {
        if (esp_timer_get_time() - start_time > HCSR04P_TIMEOUT_US) {
            return -1; // Timeout - sensor posiblemente desconectado
        }
    }
    echo_start = esp_timer_get_time();
    
    // Esperar a que el pin echo se desactive (nivel bajo)
    while (gpio_get_level(sensor->echo_pin) == 1) {
        if (esp_timer_get_time() - echo_start > HCSR04P_TIMEOUT_US) {
            return -1; // Timeout - objeto demasiado lejos o error de lectura
        }
    }
    echo_end = esp_timer_get_time();
    
    // Calcular la distancia
    float echo_duration = (echo_end - echo_start);
    distance = (echo_duration * SOUND_SPEED_CM_US) / 2.0;
    distance *= sensor->calibration_factor;
    
    sensor->distance_cm = distance;
    return distance;
}

void hcsr04p_set_calibration(hcsr04p_sensor_t *sensor, float factor) {
    if (sensor != NULL && factor > 0) {
        sensor->calibration_factor = factor;
    }
}