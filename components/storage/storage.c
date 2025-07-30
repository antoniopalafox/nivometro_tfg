#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "nivometro_sensors.h"  

static const char* TAG = "storage";                     // Etiqueta de logs para este módulo
static nvs_handle_t nvs_handle_local;                   // Handle nvs
static uint32_t record_index = 0;                       // Incrementar el índice para cada registro

void storage_init(void) {
    // Inicializa la partición nvs
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();                              // Borra la partición si está llena o hay versión distinta
        nvs_flash_init();                               // Reintenta inicializarla
    }
    nvs_open("storage", NVS_READWRITE, &nvs_handle_local);
}

void storage_buffer_data(const sensor_data_t* d) {
    char key[16];
    // Genera una clave única tipo rec0, rec1 para cada estructura
    snprintf(key, sizeof(key), "rec%lu", (unsigned long)record_index++);

    // Almacena el struct completo en nvs
    esp_err_t err = nvs_set_blob(nvs_handle_local, key, d, sizeof(sensor_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error al escribir en NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Registro guardado en %s", key);
        nvs_commit(nvs_handle_local);                  // Confirma la escritura en memoria
    }
}
