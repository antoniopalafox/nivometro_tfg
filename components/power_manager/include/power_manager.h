#pragma once                                 // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <stdbool.h>

// Enumeración para tipos de fuente de alimentación
typedef enum {
    POWER_SOURCE_UNKNOWN = 0,   // Estado desconocido o no inicializado
    POWER_SOURCE_USB,           // Alimentación por USB/Corriente externa  
    POWER_SOURCE_BATTERY        // Alimentación solo por batería
} power_source_t;

void power_manager_init(void);               // Inicializa la configuración y periféricos de gestión de energía
bool power_manager_should_sleep(void);       // Comprueba si se cumplen las condiciones para entrar en bajo consumo  
void power_manager_enter_deep_sleep(void);   // Configura y activa el deep sleep del microcontrolador

// Nuevas funciones para detección de alimentación
power_source_t power_manager_get_source(void);    // Obtiene la fuente de alimentación actual (USB o batería)
bool power_manager_is_usb_connected(void);        // Devuelve true si USB está conectado

// -----------------------------------------------------------------------------
// Estas tres funciones permiten forzar o reanudar la simulación de alimentación
// (solo cuando `simulation_enabled == true` en power_manager.c).
// -----------------------------------------------------------------------------
void power_manager_force_usb_simulation(void);     // Fuerza modo USB en simulación
void power_manager_force_battery_simulation(void); // Fuerza modo BATERÍA en simulación
void power_manager_resume_auto_simulation(void);   // Reactiva alternancia automática de simulación
