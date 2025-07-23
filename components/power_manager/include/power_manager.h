/*#pragma once                                 // Le indica al compilador que procese este fichero solo una vez por compilacion

#include <stdbool.h>
#include "esp_err.h"

// Fuentes de alimentación disponibles:
//   - POWER_SOURCE_BATTERY: modo bajo consumo (batería)
//   - POWER_SOURCE_USB:     modo normal (USB)
typedef enum {
    POWER_SOURCE_BATTERY = 0, // bajo consumo (batería)
    POWER_SOURCE_USB          // normal (USB)
} power_source_t;

// Inicializa la configuración y periféricos de gestión de energía:
//   - Registra motivo de wakeup
//   - Configura GPIO para detección de fuente
void power_manager_init(void);

// Comprueba si se cumplen las condiciones para entrar en deep sleep
bool power_manager_should_sleep(void);

// Configura y activa el deep sleep del microcontrolador
void power_manager_enter_deep_sleep(void);

// Devuelve la fuente de alimentación actual (USB o batería)
power_source_t power_manager_get_source(void);

<<<<<<< HEAD
void power_manager_init(void);               // Inicializa la configuración y periféricos de gestión de energía
bool power_manager_should_sleep(void);       // Comprueba si se cumplen las condiciones para entrar en bajo consumo  
void power_manager_enter_deep_sleep(void);   // Configura y activa el deep sleep del microcontrolador
*/
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
bool power_manager_is_usb_connected(void);        // Función de conveniencia: devuelve true si USB está conectado
=======
>>>>>>> 309a69d84d31e1a8cbc061ce05c03b7c118ea376
