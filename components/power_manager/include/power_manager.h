#pragma once                                 // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <stdbool.h>

void power_manager_init(void);               // Inicializa la configuración y periféricos de gestión de energía
bool power_manager_should_sleep(void);       // Comprueba si se cumplen las condiciones para entrar en bajo consumo  
void power_manager_enter_deep_sleep(void);   // Configura y activa el deep sleep del microcontrolador
