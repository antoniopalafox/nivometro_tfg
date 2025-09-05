// File: components/communication/include/communication.h

#pragma once    // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <esp_err.h>
#include "nivometro_sensors.h"
#include "sdkconfig.h"

// Arranca la interfaz Wi-Fi y el cliente MQTT
// Registra los manejadores de evento (connected, disconnected) para gestionar el estado de la conexión.
void communication_init(void);

// Bloquea la ejecución hasta que tanto Wi-Fi como MQTT confirmen conexión exitosa
void communication_wait_for_connection(void);

// Publica los datos de los sensores en 2 topics diferentes: sensors/ultrasonic y sensors/weight
void communication_publish(const sensor_data_t* data);

// Verifica si MQTT está conectado
bool communication_is_mqtt_connected(void);

