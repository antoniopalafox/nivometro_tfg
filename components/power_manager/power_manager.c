#include "power_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "power_manager";

// ===== VARIABLES DE SIMULACIÓN =====
static bool simulation_enabled = true;
static power_source_t simulated_source = POWER_SOURCE_USB;  // Empezar en USB
static uint32_t last_switch_time = 0;
static uint32_t switch_interval_ms = 15000; // 15 segundos entre cambios (más rápido para pruebas)
static uint32_t switch_count = 0;

// Función para obtener tiempo en milisegundos
static uint32_t get_time_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void power_manager_init(void) {
    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "🎮 MODO SIMULACIÓN ACTIVADO");
    ESP_LOGI(TAG, "🔄 Alternará automáticamente:");
    ESP_LOGI(TAG, "   USB (15s) → BATERÍA (15s) → USB...");
    ESP_LOGI(TAG, "⏱️  Observa el comportamiento diferente:");
    ESP_LOGI(TAG, "   🔌 USB: Mediciones cada 5s, SIN sleep");
    ESP_LOGI(TAG, "   🔋 BATERÍA: Mediciones cada 60s, CON sleep");
    ESP_LOGI(TAG, "==========================================");
    
    // Inicializar temporizador
    last_switch_time = get_time_ms();
    
    // FORZAR MODO USB AL INICIO PARA PRUEBAS
    simulated_source = POWER_SOURCE_USB;
    ESP_LOGI(TAG, "🔌 FORZANDO inicio en modo USB para pruebas inmediatas");
    
    // Verificar causa del despertar
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "🔋 Despertar desde deep sleep (simulando modo batería)");
            simulated_source = POWER_SOURCE_BATTERY; // Mantener modo batería tras despertar
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI(TAG, "🔌 Inicio normal del sistema (simulando modo USB)");
            simulated_source = POWER_SOURCE_USB;
            break;
    }
}

static power_source_t simulate_power_source(void) {
    uint32_t current_time = get_time_ms();
    uint32_t time_since_last = current_time - last_switch_time;
    
    // DEBUG: Log periódico del tiempo
    static uint32_t last_debug_log = 0;
    if (current_time - last_debug_log > 5000) { // Cada 5 segundos
        ESP_LOGI(TAG, "🔍 DEBUG: Tiempo transcurrido: %lu ms / %lu ms", 
                time_since_last, switch_interval_ms);
        last_debug_log = current_time;
    }
    
    // Cambiar modo cada intervalo definido
    if (time_since_last > switch_interval_ms) {
        // Alternar entre USB y BATERÍA
        power_source_t old_source = simulated_source;
        simulated_source = (simulated_source == POWER_SOURCE_USB) ? 
                          POWER_SOURCE_BATTERY : POWER_SOURCE_USB;
        
        switch_count++;
        last_switch_time = current_time;
        
        // Log del cambio con detalles
        const char* old_str = (old_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA";
        const char* new_str = (simulated_source == POWER_SOURCE_USB) ? "USB" : "BATERÍA";
        
        ESP_LOGI(TAG, "🔄 CAMBIO DE SIMULACIÓN #%lu", switch_count);
        ESP_LOGI(TAG, "   %s → %s", old_str, new_str);
        ESP_LOGI(TAG, "   Tiempo transcurrido: %lu ms", time_since_last);
        
        if (simulated_source == POWER_SOURCE_USB) {
            ESP_LOGI(TAG, "🔌 AHORA: Modo USB activo");
            ESP_LOGI(TAG, "   → Mediciones frecuentes (5s)");
            ESP_LOGI(TAG, "   → WiFi/MQTT siempre activos");
            ESP_LOGI(TAG, "   → SIN deep sleep");
        } else {
            ESP_LOGI(TAG, "🔋 AHORA: Modo BATERÍA activo");
            ESP_LOGI(TAG, "   → Mediciones espaciadas (60s)");
            ESP_LOGI(TAG, "   → Deep sleep automático");
            ESP_LOGI(TAG, "   → Máximo ahorro de energía");
        }
        
        ESP_LOGI(TAG, "⏰ Próximo cambio en 15 segundos...");
    }
    
    return simulated_source;
}

power_source_t power_manager_get_source(void) {
    if (simulation_enabled) {
        power_source_t current = simulate_power_source();
        
        // Log periódico del estado actual (cada 10 segundos)
        static uint32_t last_status_log = 0;
        uint32_t now = get_time_ms();
        if (now - last_status_log > 10000) { // 10 segundos
            const char* mode_str = (current == POWER_SOURCE_USB) ? "USB" : "BATERÍA";
            uint32_t time_to_switch = (switch_interval_ms - (now - last_switch_time)) / 1000;
            ESP_LOGI(TAG, "📊 Estado: %s | Cambio en: %lu segundos", mode_str, time_to_switch);
            last_status_log = now;
        }
        
        return current;
    }
    
    // Lógica real cuando tengas el hardware
    return POWER_SOURCE_BATTERY;
}

bool power_manager_should_sleep(void) {
    power_source_t source = power_manager_get_source();
    
    if (source == POWER_SOURCE_USB) {
        ESP_LOGD(TAG, "🔌 USB detectado - mantener modo activo");
        return false;
    } else {
        // BATERÍA: Usar la lógica original de tu código
        static int cycle_count = 0;
        cycle_count++;
        
        if (cycle_count >= 1) {
            ESP_LOGI(TAG, "🔋 Solo batería detectada - power_manager: should_sleep == true");
            return true;
        }
        return false;
    }
}

void power_manager_enter_deep_sleep(void) {
    power_source_t source = power_manager_get_source();
    
    if (source == POWER_SOURCE_USB) {
        ESP_LOGW(TAG, "⚠️  USB conectado - cancelando deep sleep");
        return;
    }
    
    ESP_LOGI(TAG, "💤 SIMULACIÓN: Entrando en deep sleep...");
    ESP_LOGI(TAG, "⏰ Sleep por 15 segundos (en modo real serían 30s)");
    ESP_LOGI(TAG, "🔄 Al despertar continuará la simulación...");
    
    // Sleep más corto para pruebas más ágiles
    const uint64_t SIMULATION_SLEEP_TIME = 15000000ULL; // 15 segundos
    esp_sleep_enable_timer_wakeup(SIMULATION_SLEEP_TIME);
    esp_deep_sleep_start();
}

bool power_manager_is_usb_connected(void) {
    return (power_manager_get_source() == POWER_SOURCE_USB);
}

// ===== FUNCIONES ADICIONALES PARA DEBUGGING =====

void power_manager_log_status(void) {
    power_source_t source = power_manager_get_source();
    const char* mode_str = (source == POWER_SOURCE_USB) ? "USB/Corriente" : "Batería";
    const char* behavior_str = (source == POWER_SOURCE_USB) ? "ACTIVO CONTINUO" : "AHORRO ENERGÍA";
    
    ESP_LOGI(TAG, "📊 ESTADO SIMULACIÓN:");
    ESP_LOGI(TAG, "   Fuente: %s", mode_str);
    ESP_LOGI(TAG, "   Modo: %s", behavior_str);
    ESP_LOGI(TAG, "   Cambios realizados: %lu", switch_count);
    ESP_LOGI(TAG, "   Simulación: %s", simulation_enabled ? "ACTIVA" : "MANUAL");
}

// Funciones para control manual (para futuras pruebas)
void power_manager_force_usb_simulation(void) {
    simulation_enabled = false;
    simulated_source = POWER_SOURCE_USB;
    ESP_LOGI(TAG, "🔌 MANUAL: Forzando modo USB");
}

void power_manager_force_battery_simulation(void) {
    simulation_enabled = false;
    simulated_source = POWER_SOURCE_BATTERY;
    ESP_LOGI(TAG, "🔋 MANUAL: Forzando modo BATERÍA");
}

void power_manager_resume_auto_simulation(void) {
    simulation_enabled = true;
    last_switch_time = get_time_ms();
    ESP_LOGI(TAG, "🔄 Simulación automática reactivada");
}