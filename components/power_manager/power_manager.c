<<<<<<< HEAD
/*
=======
// components/power_manager/src/power_manager.c
>>>>>>> 309a69d84d31e1a8cbc061ce05c03b7c118ea376
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
<<<<<<< HEAD
}*/
#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "power_manager";               // Etiqueta de logs para este modulo

// Configuración de hardware - AJUSTA ESTE PIN SEGÚN TU CIRCUITO
#define POWER_DETECT_PIN    GPIO_NUM_34  // Pin para detectar USB vs Batería
#define DEBOUNCE_TIME_MS    50           // Tiempo anti-rebote en ms

// Variables estáticas
static power_source_t current_power_source = POWER_SOURCE_UNKNOWN;
static bool gpio_initialized = false;

/**
 * Inicializar GPIO para detección de alimentación
 */
static void init_power_detection_gpio(void) {
    if (gpio_initialized) {
        return;
    }
    
    gpio_config_t power_detect_config = {
        .pin_bit_mask = (1ULL << POWER_DETECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&power_detect_config);
    if (ret == ESP_OK) {
        gpio_initialized = true;
        ESP_LOGI(TAG, "GPIO %d configurado para detección de alimentación", POWER_DETECT_PIN);
    } else {
        ESP_LOGE(TAG, "Error configurando GPIO %d: %s", POWER_DETECT_PIN, esp_err_to_name(ret));
    }
}

/**
 * Detectar fuente de alimentación actual
 */
static power_source_t detect_power_source(void) {
    if (!gpio_initialized) {
        init_power_detection_gpio();
    }
    
    // Hacer lecturas con anti-rebote
    int reading1 = gpio_get_level(POWER_DETECT_PIN);
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
    int reading2 = gpio_get_level(POWER_DETECT_PIN);
    
    if (reading1 == reading2) {
        // AJUSTA ESTA LÓGICA SEGÚN TU CIRCUITO:
        // HIGH (1) = USB/Corriente externa conectada
        // LOW (0) = Solo batería
        return (reading1 == 1) ? POWER_SOURCE_USB : POWER_SOURCE_BATTERY;
    }
    
    // Si las lecturas son inconsistentes, mantener estado anterior
    return current_power_source;
}

void power_manager_init(void) {
    // Inicializar GPIO para detección
    init_power_detection_gpio();
    
    // Detectar fuente de alimentación inicial
    current_power_source = detect_power_source();
    
    // Al arrancar, comprueba si viene de deep_sleep y lo registra
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep (timer) - Fuente: %s", 
                (current_power_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA");
    } else if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Power on or reset - Fuente: %s", 
                (current_power_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA");
    }
}

bool power_manager_should_sleep(void) {
    // Actualizar detección de fuente de alimentación
    power_source_t new_source = detect_power_source();
    
    // Informar cambios de alimentación
    if (new_source != current_power_source && new_source != POWER_SOURCE_UNKNOWN) {
        ESP_LOGI(TAG, "Cambio de alimentación: %s → %s",
                (current_power_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA",
                (new_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA");
        current_power_source = new_source;
    }
    
    if (current_power_source == POWER_SOURCE_USB) {
        // CONECTADO A USB: No entrar en sleep - permanecer activo
        ESP_LOGD(TAG, "USB conectado - mantener modo activo");
        return false;
    } else {
        // SOLO BATERÍA: Entrar en sleep después del primer ciclo
        static int cycle_count = 0;
        cycle_count++;
        if (cycle_count >= 1) {
            ESP_LOGI(TAG, "Solo batería detectada - power_manager: should_sleep == true");
            return true;
        }
        return false;
    }
}

void power_manager_enter_deep_sleep(void) {
    // Verificar una vez más la fuente antes de entrar en sleep
    power_source_t source = detect_power_source();
    
    if (source == POWER_SOURCE_USB) {
        ESP_LOGW(TAG, "USB conectado - cancelando deep sleep");
        return;
    }
    
    // Programa un wakeup por temporizador y arranca deep sleep
    const uint64_t WAKEUP_TIME_SEC = 30ULL; // 30 segundos
    ESP_LOGI(TAG, "Entering deep sleep for %llu seconds...", WAKEUP_TIME_SEC);
    esp_sleep_enable_timer_wakeup(WAKEUP_TIME_SEC * 1000000ULL);
    esp_deep_sleep_start();
}

// NUEVAS FUNCIONES AÑADIDAS:

power_source_t power_manager_get_source(void) {
    // Actualizar y devolver fuente actual
    power_source_t new_source = detect_power_source();
    if (new_source != POWER_SOURCE_UNKNOWN) {
        current_power_source = new_source;
    }
    return current_power_source;
}

bool power_manager_is_usb_connected(void) {
    return (power_manager_get_source() == POWER_SOURCE_USB);
}
=======
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
>>>>>>> 309a69d84d31e1a8cbc061ce05c03b7c118ea376
