//File: main/main.c

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "nivometro_sensors.h"
#include "diagnostics.h"
#include "config.h"
#include "storage.h"
#include "communication.h"
#include "power_manager.h"
#include "utils.h"
#include "tasks.h"

static const char *TAG = "NIVOMETRO_MAIN";

// --- Configuración sensores ---
#define HCSR04P_TRIGGER_PIN         12
#define HCSR04P_ECHO_PIN            13
#define HCSR04P_CAL_FACTOR          1.02f

#define HX711_DOUT_PIN              GPIO_NUM_26
#define HX711_SCK_PIN               GPIO_NUM_27
#define HX711_KNOWN_WEIGHT_G        500.0f

// Instancia global del nivómetro
nivometro_t g_nivometro;


// Modo calibración

static void run_calibration_mode(void) {
    ESP_LOGI(TAG, "===============ENTRANDO EN MODO CALIBRACIÓN===============");
    
    // Cambiar LED a modo calibración (parpadeo rápido)
    led_set_state(LED_STATE_CALIBRATION);
    
    // Limpiar la partición NVS para máximo espacio disponible
    esp_err_t clear_result = calibration_all_nvs_partition();
        
    // Mostrar parámetros configurados
    ESP_LOGI(TAG, "Parámetros de calibración (desde menuconfig):");
    ESP_LOGI(TAG, "Peso conocido HX711: %d gramos", CONFIG_CALIBRATION_HX711_KNOWN_WEIGHT);
    ESP_LOGI(TAG, "Muestras HX711: %d", CONFIG_CALIBRATION_HX711_SAMPLES);
    ESP_LOGI(TAG, "Tolerancia HX711: ±%d%%", CONFIG_CALIBRATION_HX711_TOLERANCE_PERCENT);
    ESP_LOGI(TAG, "Distancia conocida HC-SR04P: %d cm", CONFIG_CALIBRATION_HCSR04P_KNOWN_DISTANCE);
    ESP_LOGI(TAG, "Muestras HC-SR04P: %d", CONFIG_CALIBRATION_HCSR04P_SAMPLES);
    ESP_LOGI(TAG, "Tolerancia HC-SR04P: ±%d%%", CONFIG_CALIBRATION_HCSR04P_TOLERANCE_PERCENT);
    
    calibration_data_t cal_data = {0};
    cal_data.magic_number = CALIBRATION_MAGIC_NUMBER;
    cal_data.calibrated = false;
    cal_data.known_weight_used = (float)CONFIG_CALIBRATION_HX711_KNOWN_WEIGHT;
    cal_data.known_distance_used = (float)CONFIG_CALIBRATION_HCSR04P_KNOWN_DISTANCE;
    cal_data.calibration_timestamp = get_timestamp_seconds();
    cal_data.hcsr04p_cal_factor = HCSR04P_CAL_FACTOR; // Valor inicial
    
    // === PASO 1: TARA HX711 ===
    ESP_LOGI(TAG, "PASO 1: Calibración HX711 - TARA");
    ESP_LOGI(TAG, "El sistema tomará %d mediciones", CONFIG_CALIBRATION_HX711_SAMPLES);
    ESP_LOGI(TAG, "INSTRUCCIONES:");
    ESP_LOGI(TAG, "1. Asegúrate de que la balanza esté VACÍA");
    ESP_LOGI(TAG, "2. Presiona BOOT para continuar");
    
    boot_button_wait_for_press();
    
    esp_err_t tare_result = nivometro_tare_scale(&g_nivometro);
    if (tare_result == ESP_OK) {
        ESP_LOGI(TAG, "Tara completada correctamente");
        cal_data.hx711_offset = g_nivometro.scale.offset;
        cal_data.calibrated = false; 
        esp_err_t tare_save_result = calibration_save_to_nvs(&cal_data);
        if (tare_save_result == ESP_OK) {
            ESP_LOGI(TAG, "Tara guardada en NVS");
        } else {
            ESP_LOGW(TAG, "Error guardando tara: %s", esp_err_to_name(tare_save_result));
        }
    } else {
        ESP_LOGE(TAG, "Error en tara: %s", esp_err_to_name(tare_result));
        led_set_state(LED_STATE_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // === PASO 2: CALIBRACIÓN PESO HX711 ===
    ESP_LOGI(TAG, "PASO 2: Calibración HX711 - PESO CONOCIDO");
    ESP_LOGI(TAG, "El sistema tomará %d mediciones", CONFIG_CALIBRATION_HX711_SAMPLES);
    ESP_LOGI(TAG, "INSTRUCCIONES:");
    ESP_LOGI(TAG, "1. Coloca un peso conocido de %d gramos", CONFIG_CALIBRATION_HX711_KNOWN_WEIGHT);
    ESP_LOGI(TAG, "2. Presiona BOOT para continuar");
    
    boot_button_wait_for_press();
    
    esp_err_t weight_cal_result = nivometro_calibrate_scale_with_validation(
        &g_nivometro, 
        (float)CONFIG_CALIBRATION_HX711_KNOWN_WEIGHT,
        (float)CONFIG_CALIBRATION_HX711_TOLERANCE_PERCENT
    );
    
    if (weight_cal_result == ESP_OK) {
        ESP_LOGI(TAG, "Calibración de peso completada");
        cal_data.hx711_scale_factor = g_nivometro.scale.scale;
    } else {
        ESP_LOGE(TAG, "Error en calibración de peso");
        led_set_state(LED_STATE_ERROR);
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    
    // === PASO 3: CALIBRACIÓN DISTANCIA HC-SR04P ===
    ESP_LOGI(TAG, "PASO 3: Calibración HC-SR04P");
    ESP_LOGI(TAG, "INSTRUCCIONES:");
    ESP_LOGI(TAG, "1. Coloca un objeto a exactamente %d cm del sensor", CONFIG_CALIBRATION_HCSR04P_KNOWN_DISTANCE);
    ESP_LOGI(TAG, "2. Asegúrate de que el objeto esté perpendicular al sensor");
    ESP_LOGI(TAG, "3. El sistema tomará %d mediciones", CONFIG_CALIBRATION_HCSR04P_SAMPLES);
    ESP_LOGI(TAG, "4. Presiona BOOT para continuar");
    
    boot_button_wait_for_press();
    
    float new_cal_factor = nivometro_calibrate_ultrasonic(
        &g_nivometro,
        (float)CONFIG_CALIBRATION_HCSR04P_KNOWN_DISTANCE,
        CONFIG_CALIBRATION_HCSR04P_SAMPLES,
        (float)CONFIG_CALIBRATION_HCSR04P_TOLERANCE_PERCENT
    );
    
    if (new_cal_factor > 0) {
        ESP_LOGI(TAG, "Calibración HC-SR04P completada");
        cal_data.hcsr04p_cal_factor = new_cal_factor;
    } else {
        ESP_LOGW(TAG, "Problema en calibración HC-SR04P, usando factor por defecto");
        cal_data.hcsr04p_cal_factor = HCSR04P_CAL_FACTOR;
    }
    
    // Guardar datos de calibración finales
    cal_data.calibrated = true;
    esp_err_t save_result = calibration_save_to_nvs(&cal_data);
    
    if (save_result == ESP_OK) {
        ESP_LOGI(TAG, "Calibraciones guardadas en NVS correctamente");
    } else {
        ESP_LOGE(TAG, "Error guardando calibraciones: %s", esp_err_to_name(save_result));
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "CALIBRACIÓN COMPLETADA CON ÉXITO");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Resumen de calibración:");
    ESP_LOGI(TAG, "Peso usado: %.1f g → Factor: %.6f", cal_data.known_weight_used, cal_data.hx711_scale_factor);
    ESP_LOGI(TAG, "Distancia usada: %.1f cm → Factor: %.6f", cal_data.known_distance_used, cal_data.hcsr04p_cal_factor);
    ESP_LOGI(TAG, "Datos guardados automáticamente en NVS");
    ESP_LOGI(TAG, "Reiniciando en modo nominal en %d segundos...", CONFIG_CALIBRATION_CONFIRMATION_TIMEOUT_S);
    
    // LED encendido fijo para indicar finalización
    led_set_state(LED_STATE_SOLID_ON);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_CALIBRATION_CONFIRMATION_TIMEOUT_S * 1000));
        
    esp_restart();
}

// FUNCIÓN PRINCIPAL
void app_main(void) {
    esp_err_t ret;

    // 1) Inicializar GPIO para LED y botón BOOT 
    led_init();
    boot_button_init();
    led_start_task();
    led_set_state(LED_STATE_OFF);

    // 2) Logs y diagnóstico
    diagnostics_init();

    // 3) Mensajes de arranque del nivómetro
    ESP_LOGI(TAG, "Iniciando TFG Nivómetro Antártida");
    ESP_LOGI(TAG, "Modo de alimentación");
    ESP_LOGI(TAG, "USB conectado = Modo Nominal | Solo Batería = Deep Sleep");

    // 4) Inicialización explícita de NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 5) Modo calibración
    if (boot_button_check_calibration_mode()) {
        ESP_LOGI(TAG, "Botón BOOT detectado - Entrando en modo calibración");
        
        // Inicializar solo lo esencial para calibración
        timer_manager_init();
        
        // Inicializar nivómetro para calibración
        nivometro_config_t nivometro_config = {
            .hcsr04p_trigger_pin = HCSR04P_TRIGGER_PIN,
            .hcsr04p_echo_pin    = HCSR04P_ECHO_PIN,
            .hcsr04p_cal_factor  = HCSR04P_CAL_FACTOR,
            .hx711_dout_pin      = HX711_DOUT_PIN,
            .hx711_sck_pin       = HX711_SCK_PIN,
            .hx711_gain          = HX711_GAIN_128,
            .hx711_known_weight  = HX711_KNOWN_WEIGHT_G
        };
        
        ret = nivometro_init(&g_nivometro, &nivometro_config);
        ESP_ERROR_CHECK(ret);
        
        run_calibration_mode();
        
    }
    
    //Continua con el arranque normal
    ESP_LOGI(TAG, "Arranque normal");

    // 6) Configuración global
    config_init();

    // 7) Validación de calibración
    bool calibration_valid = calibration_check_and_warn();
    
    // 8) Configurar LED según estado de calibración
    if (calibration_valid) {
        led_set_state(LED_STATE_NORMAL);
        ESP_LOGI(TAG, "LED configurado: parpadeo lento - Sistema calibrado");
    } else {
        led_set_state(LED_STATE_WARNING);
        ESP_LOGI(TAG, "LED configurado: parpadeo medio - Requiere calibración");
    }
    
    // 9) Inicializar el nivómetro 
    nivometro_config_t nivometro_config = {
        .hcsr04p_trigger_pin = HCSR04P_TRIGGER_PIN,
        .hcsr04p_echo_pin    = HCSR04P_ECHO_PIN,
        .hcsr04p_cal_factor  = HCSR04P_CAL_FACTOR,
        .hx711_dout_pin      = HX711_DOUT_PIN,
        .hx711_sck_pin       = HX711_SCK_PIN,
        .hx711_gain          = HX711_GAIN_128,
        .hx711_known_weight  = HX711_KNOWN_WEIGHT_G 
    };
    ret = nivometro_init(&g_nivometro, &nivometro_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando nivómetro: %s", esp_err_to_name(ret));
        led_set_state(LED_STATE_ERROR);
        return;
    }
    
    // 10)Aplicar calibración desde NVS
    calibration_data_t cal_data = {0};
    if (calibration_load_from_nvs(&cal_data) == ESP_OK) {
        calibration_apply_to_sensors(&g_nivometro, &cal_data);
        ESP_LOGI(TAG, "Calibraciones aplicadas desde NVS");
        
        char summary[200];
        format_calibration_summary(&cal_data, summary, sizeof(summary));
        ESP_LOGI(TAG, "%s", summary);
    } else {
        ESP_LOGW(TAG, "Usando valores de calibración por defecto");
    }
    
    ESP_LOGI(TAG, "Nivómetro inicializado correctamente");

    // 11) Almacenamiento local
    storage_init();

    // 12) Comunicaciones (Wi-Fi, MQTT, sincronización de hora)
    communication_init();
    ESP_LOGI(TAG, "Comunicaciones inicializadas");

    // 13) GESTIÓN DE ENERGÍA CON DETECCIÓN USB/BATERÍA
    power_manager_init();
    //power_manager_force_battery_simulation();  // Descomentar para forzar simulación de batería
    
    if (power_manager_is_usb_connected()) {
        ESP_LOGI(TAG, "USB Conectado - Iniciando en modo nominal");
    } else {
        ESP_LOGI(TAG, "SOLO BATERÍA - Iniciando en modo batería");
        ESP_LOGI(TAG, "Deep_sleep automático cada 30s");
    }

    // 14) Temporizador interno
    timer_manager_init();

    // 15) Log de configuración detallada
    ESP_LOGI(TAG, "Todos los sensores inicializados correctamente");
    ESP_LOGI(TAG, "Configuración del sistema:");
    ESP_LOGI(TAG, "Power Management: GPIO 34 para detección USB/Batería");

    // 17) ARRANCAR TAREAS CON GESTIÓN INTELIGENTE DE ENERGÍA
    tasks_start_all();

    ESP_LOGI(TAG, "Sistema iniciado completamente");
    if (!calibration_valid) {
        ESP_LOGW(TAG, "RECORDATORIO: Para calibrar, reinicia manteniendo BOOT presionado");
    }
}