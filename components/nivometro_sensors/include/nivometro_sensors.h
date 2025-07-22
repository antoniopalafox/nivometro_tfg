#pragma once

#include "hcsr04p.h"
#include "hx711.h"
#include "vl53l0x.h"
#include "esp_err.h"
#include "esp_log.h"

// Estructura de datos unificada del nivómetro
typedef struct {
    // Datos de sensores
    float ultrasonic_distance_cm;    // HC-SR04P
    float laser_distance_mm;         // VL53L0X  
    float weight_grams;              // HX711
    
    // Metadatos
    uint64_t timestamp_us;
    uint8_t sensor_status;           // Bits: [2]=VL53L0X, [1]=HX711, [0]=HC-SR04P
    float battery_voltage;
    int8_t temperature_c;            // Temperatura estimada
} nivometro_data_t;

// AGREGADO: Estructura de datos para comunicación entre componentes
typedef struct {
    float distance_cm;        // Distancia HC-SR04P en centímetros
    float weight_kg;          // Peso HX711 en kilogramos  
    float laser_mm;           // Distancia VL53L0X en milímetros
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
    
    // I2C VL53L0X
    i2c_port_t vl53l0x_i2c_port;
    uint8_t vl53l0x_address;
    vl53l0x_accuracy_t vl53l0x_accuracy;
    float vl53l0x_cal_factor;
} nivometro_config_t;

// Estructura principal del nivómetro
typedef struct {
    hcsr04p_sensor_t ultrasonic;
    hx711_sensor_t scale;
    vl53l0x_sensor_t laser;
    nivometro_config_t config;
    bool initialized;
} nivometro_t;

// Funciones principales
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

// AGREGADO: Función de conversión entre estructuras
void nivometro_data_to_sensor_data(const nivometro_data_t *src, sensor_data_t *dst);