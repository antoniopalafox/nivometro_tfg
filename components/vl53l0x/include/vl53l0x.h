// vl53l0x.h
#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define VL53L0X_DEFAULT_ADDRESS 0x29

// Modos de medición
typedef enum {
    VL53L0X_MODE_SINGLE = 0,       // Medición única
    VL53L0X_MODE_CONTINUOUS = 1,   // Medición continua
    VL53L0X_MODE_TIMED = 2         // Medición temporizada
} vl53l0x_mode_t;

// Precisión/velocidad
typedef enum {
    VL53L0X_ACCURACY_GOOD = 0,     // Buena precisión, más lento
    VL53L0X_ACCURACY_BETTER = 1,   // Mejor precisión, más lento
    VL53L0X_ACCURACY_BEST = 2,     // Mejor precisión, más lento
    VL53L0X_ACCURACY_FAST = 3,     // Precisión reducida, más rápido
    VL53L0X_ACCURACY_FASTER = 4    // Precisión reducida, muy rápido
} vl53l0x_accuracy_t;

typedef struct {
    i2c_port_t i2c_port;       // Puerto I2C
    uint8_t address;           // Dirección I2C
    uint16_t timeout_ms;       // Timeout en ms
    float calibration_factor;  // Factor de calibración
    vl53l0x_mode_t mode;       // Modo de medición
    vl53l0x_accuracy_t accuracy; // Configuración de precisión
} vl53l0x_sensor_t;

/**
 * @brief Inicializa el sensor VL53L0X
 * @param sensor Puntero a la estructura del sensor
 * @param i2c_port Puerto I2C
 * @param address Dirección I2C del sensor
 * @return true si la inicialización fue exitosa
 */
bool vl53l0x_init(vl53l0x_sensor_t *sensor, i2c_port_t i2c_port, uint8_t address);

/**
 * @brief Lee la distancia del sensor
 * @param sensor Puntero a la estructura del sensor
 * @return Distancia en milímetros o 0 si hay error
 */
uint16_t vl53l0x_read_distance(vl53l0x_sensor_t *sensor);

/**
 * @brief Configura el modo de medición
 * @param sensor Puntero a la estructura del sensor
 * @param mode Modo de medición
 * @return true si la configuración fue exitosa
 */
bool vl53l0x_set_mode(vl53l0x_sensor_t *sensor, vl53l0x_mode_t mode);

/**
 * @brief Configura la precisión/velocidad de medición
 * @param sensor Puntero a la estructura del sensor
 * @param accuracy Nivel de precisión deseado
 * @return true si la configuración fue exitosa
 */
bool vl53l0x_set_accuracy(vl53l0x_sensor_t *sensor, vl53l0x_accuracy_t accuracy);

/**
 * @brief Configura el factor de calibración
 * @param sensor Puntero a la estructura del sensor
 * @param factor Factor de calibración
 */
void vl53l0x_set_calibration(vl53l0x_sensor_t *sensor, float factor);

/**
 * @brief Despierta el sensor del modo de bajo consumo
 * @param sensor Puntero a la estructura del sensor
 * @return true si fue exitoso
 */
bool vl53l0x_wake_up(vl53l0x_sensor_t *sensor);

/**
 * @brief Pone el sensor en modo de bajo consumo
 * @param sensor Puntero a la estructura del sensor
 * @return true si fue exitoso
 */
bool vl53l0x_sleep(vl53l0x_sensor_t *sensor);

#endif // VL53L0X_H