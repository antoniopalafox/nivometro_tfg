//File: components/diagnotics/include/diagnotics.h

#pragma once                   // Le indica al compilador que procese este fichero solo una vez por compilacion

#include "esp_err.h"

void diagnostics_init(void);                                                        // Inicializa el sistema de logging

void diagnostics_log_error(const char *tag, esp_err_t err, const char *msg);        // Registra un error con su codigo y descripcion

void diagnostics_record_event(const char *event_name, const char *details);         // Guarda un evento significativo con más detalles

