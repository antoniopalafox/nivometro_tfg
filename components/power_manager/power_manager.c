#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char* TAG = "power_manager";               // Etiqueta de logs para este modulo

void power_manager_init(void) {
    // Al arrancar, comprueba si viene de deep_sleep y lo registra
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep (timer)");
    } else if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Power on or reset");
    }
}

bool power_manager_should_sleep(void) {
    // Controla un contador de ciclos y devuelve true tras cada ciclo
    static int cycle_count = 0;
    cycle_count++;
    if (cycle_count >= 1) {
        ESP_LOGI(TAG, "power_manager: should_sleep == true");
        return true;
    }
    return false;
}

void power_manager_enter_deep_sleep(void) {
    // Programa un wakeup por temporizador y arranca deep sleep
    const uint64_t WAKEUP_TIME_SEC = 30ULL; // 30 segundos
    ESP_LOGI(TAG, "Entering deep sleep for %llu seconds...", WAKEUP_TIME_SEC);
    esp_sleep_enable_timer_wakeup(WAKEUP_TIME_SEC * 1000000ULL);
    esp_deep_sleep_start();
}