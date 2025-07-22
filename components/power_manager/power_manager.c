// components/power_manager/src/power_manager.c
#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char* TAG = "power_manager";  // Etiqueta de logs para este módulo

// GPIO donde conectas el divisor de tensión desde VIN (5V USB)
#define PIN_POWER_DETECT   GPIO_NUM_17

void power_manager_init(void) {
    // Al arrancar, comprueba si viene de deep_sleep y lo registra
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep (timer)");
    } else if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Power on or reset");
    }

    // Configuración GPIO para detección de fuente de alimentación
    gpio_config_t cfg = {
        .pin_bit_mask   = 1ULL << PIN_POWER_DETECT,
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = GPIO_PULLUP_DISABLE,
        .pull_down_en   = GPIO_PULLDOWN_ENABLE,
        .intr_type      = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

bool power_manager_should_sleep(void) {
    // Controla un contador de ciclos y devuelve true tras el primer ciclo
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

power_source_t power_manager_get_source(void) {
    // Lee el nivel del pin para detectar fuente: 1=USB, 0=batería
    int level = gpio_get_level(PIN_POWER_DETECT);
    power_source_t src = (level == 1)
        ? POWER_SOURCE_USB
        : POWER_SOURCE_BATTERY;
    ESP_LOGI(TAG, "Detected power source: %s",
             src == POWER_SOURCE_USB ? "USB (normal)" : "Battery (low power)");
    return src;
}
