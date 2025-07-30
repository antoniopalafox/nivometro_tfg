#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <inttypes.h>  // Para PRIu32

static const char* TAG = "power_manager";

// -------------------------------------------------------------------------
// Para cambiar entre MODO SIMULACIÓN y MODO REAL,
// ajusta la variable `simulation_enabled` a `true` o `false`.
// -------------------------------------------------------------------------
static bool simulation_enabled = false;  // false = power_online, true = power_battery
// -------------------------------------------------------------------------

// ===== VARIABLES DE SIMULACIÓN =====
static power_source_t simulated_source   = POWER_SOURCE_USB;
static uint32_t       last_switch_time   = 0;
static const uint32_t switch_interval_ms = 15000;  // 15 s para pruebas
static uint32_t       switch_count       = 0;

// Pin HW para detección de USB en modo real (ajústalo a tu diseño)
#define USB_DETECT_PIN  GPIO_NUM_4

// Obtener tiempo en ms
static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void power_manager_init(void) {
    if (simulation_enabled) {
        ESP_LOGI(TAG, "==========================================");
        ESP_LOGI(TAG, "🎮 MODO SIMULACIÓN ACTIVADO");
        ESP_LOGI(TAG, "🔄 Alternará automáticamente:");
        ESP_LOGI(TAG, "   USB (%" PRIu32 " s) → BATERÍA (%" PRIu32 " s) → USB...",
                 switch_interval_ms/1000, switch_interval_ms/1000);
        ESP_LOGI(TAG, "⏱️  Observa el comportamiento diferente:");
        ESP_LOGI(TAG, "   🔌 USB: Mediciones cada 5s, SIN sleep");
        ESP_LOGI(TAG, "   🔋 BATERÍA: Mediciones cada 60s, CON sleep");
        ESP_LOGI(TAG, "==========================================");

        last_switch_time = get_time_ms();
        simulated_source  = POWER_SOURCE_USB;
        ESP_LOGI(TAG, "🔌 FORZANDO inicio en modo USB para pruebas inmediatas");

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            simulated_source = POWER_SOURCE_BATTERY;
            ESP_LOGI(TAG, "🔋 Despertar desde deep sleep (simulando modo BATERÍA)");
        } else {
            simulated_source = POWER_SOURCE_USB;
            ESP_LOGI(TAG, "🔌 Inicio normal (simulando modo USB)");
        }
    } else {
        // MODO REAL: configurar GPIO de detección USB
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << USB_DETECT_PIN,
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE
        };
        gpio_config(&cfg);
        ESP_LOGI(TAG, "🔌 MODO REAL ACTIVADO: detectando USB en pin %d", USB_DETECT_PIN);
    }
}

// Alterna la fuente simulada
static power_source_t simulate_power_source(void) {
    uint32_t now = get_time_ms();
    if (now - last_switch_time > switch_interval_ms) {
        power_source_t old = simulated_source;
        simulated_source = (old == POWER_SOURCE_USB) ? POWER_SOURCE_BATTERY : POWER_SOURCE_USB;
        last_switch_time = now;
        switch_count++;
        ESP_LOGI(TAG, "🔄 Simulación #%" PRIu32 ": %s → %s",
                 switch_count,
                 old == POWER_SOURCE_USB ? "USB" : "BATERÍA",
                 simulated_source == POWER_SOURCE_USB ? "USB" : "BATERÍA");
    }
    return simulated_source;
}

power_source_t power_manager_get_source(void) {
    if (simulation_enabled) {
        return simulate_power_source();
    } else {
        int level = gpio_get_level(USB_DETECT_PIN);
        return level ? POWER_SOURCE_USB : POWER_SOURCE_BATTERY;
    }
}

bool power_manager_should_sleep(void) {
    return (power_manager_get_source() != POWER_SOURCE_USB);
}

void power_manager_enter_deep_sleep(void) {
    if (power_manager_get_source() == POWER_SOURCE_USB) {
        ESP_LOGW(TAG, "🔌 USB conectado: cancelando deep sleep");
        return;
    }
    ESP_LOGI(TAG, "💤 Entrando en deep sleep...");
    const uint64_t SLEEP_US = 30ULL * 1000000ULL;  // 30 s real
    esp_sleep_enable_timer_wakeup(SLEEP_US);
    esp_deep_sleep_start();
}

bool power_manager_is_usb_connected(void) {
    return (power_manager_get_source() == POWER_SOURCE_USB);
}

// Control manual en simulación
/*void power_manager_force_usb_simulation(void) {
    if (simulation_enabled) {
        simulated_source = POWER_SOURCE_USB;
        last_switch_time = get_time_ms();
        ESP_LOGI(TAG, "🔌 MANUAL: Forzando modo USB");
    }
}

void power_manager_force_battery_simulation(void) {
    if (simulation_enabled) {
        simulated_source = POWER_SOURCE_BATTERY;
        last_switch_time = get_time_ms();
        ESP_LOGI(TAG, "🔋 MANUAL: Forzando modo BATERÍA");
    }
}*/
