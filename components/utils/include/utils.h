#pragma once                               // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "nivometro_sensors.h"   

void timer_manager_init(void);              // Inicializa el gestor de temporizadores para usar timer_manager_delay_ms()

void timer_manager_delay_ms(uint32_t ms);   // Retrasa la ejecución de la tarea actual 
  
int data_formatter_format_json(const sensor_data_t *data, char *buf, size_t bufsize);   // Serializa los datos de sensores como json en buf
