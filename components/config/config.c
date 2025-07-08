#include "config.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char* TAG = "config";                     // Etiqueta que usará esp_logx para clasificar mensajes de este módulo

void config_init(void) {
    // Inicializa la nvs (memoria no volátil) para que el resto del sistema pueda leer/escribir parámetros sin fallos.
    esp_err_t err = nvs_flash_init();

    // Si la nvs está llena o la versión de la partición ha cambiado, se borra y se inicializa de nuevo.
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Borra toda la partición NVS
        nvs_flash_erase();
        // Vuelve a inicializarla para dejarla lista
        nvs_flash_init();
    }
    
    ESP_LOGI(TAG, "config_init() called, no CONFIG_* values available");
}


