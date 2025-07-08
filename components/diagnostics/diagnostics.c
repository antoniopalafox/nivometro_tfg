#include "diagnostics.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "diagnostics";                             // Etiqueta de logs para este módulo
static nvs_handle_t diag_nvs_handle;                                // Handle nvs para diagnósticos

void diagnostics_init(void)
{
    // Inicializa la partición nvs y abre el namespace diag
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES
     || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return;
    }

     // Abre (o crea) el namespace diag en nvs para guardar errores/eventos
    err = nvs_open("diag", NVS_READWRITE, &diag_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open diag failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Diagnostics initialized");
    }
}

void diagnostics_log_error(const char *subsystem, esp_err_t err, const char *msg)
{
    // Registra el error en consola y, si nvs está abierto, aumenta el contador y guarda el mensaje
    ESP_LOGE(subsystem, "Error %s: %s", msg, esp_err_to_name(err));
    if (diag_nvs_handle) {
        uint32_t cnt = 0;
        nvs_get_u32(diag_nvs_handle, "err_cnt", &cnt);
        cnt++;
        nvs_set_u32(diag_nvs_handle, "err_cnt", cnt);
        nvs_set_str(diag_nvs_handle, "last_err", msg);
        nvs_commit(diag_nvs_handle);
    }
}

void diagnostics_record_event(const char *event_name, const char *details)
{
    // Registra el evento en consola y, si nvs está abierto, lo guarda incrementando el límite
    ESP_LOGI(TAG, "Event %s: %s", event_name, details ? details : "");

    if (diag_nvs_handle) {
        uint32_t idx = 0;
        nvs_get_u32(diag_nvs_handle, "evt_cnt", &idx);

        char key[16];
        snprintf(key, sizeof(key), "evt_%" PRIu32, idx);

        nvs_set_str(diag_nvs_handle, key, event_name);
        idx++;
        nvs_set_u32(diag_nvs_handle, "evt_cnt", idx);
        nvs_commit(diag_nvs_handle);
    }
}
