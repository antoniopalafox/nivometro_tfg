#pragma once                                             // Le indica al compilador que procese este fichero solo una vez por compilacion

#include "nivometro_sensors.h"
#include "esp_err.h"

void storage_init(void);                                 // Inicializa el sistema de almacenamiento (nvs, etc)
void storage_buffer_data(const sensor_data_t* data);     // Guarda temporalmente los datos de sensores para su posterior envío



