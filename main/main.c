// tfg/main/main.c (VERSIÓN COMPLETA: LOGS + MQTT)
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/i2c.h"
#include "esp_timer.h"

// CAMBIADO: Usar el componente nivometro_sensors en lugar de drivers directos
#include "nivometro_sensors.h"
#include "tasks.h"

// Incluir componentes base (si existen)
#ifdef CONFIG_ENABLE_MQTT
#include "communication.h"
#endif

static const char *TAG = "NIVOMETRO_MAIN";

// Configuración I2C
#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21  
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000

// Configuración sensores
#define HCSR04P_TRIGGER_PIN         12
#define HCSR04P_ECHO_PIN            13
#define HCSR04P_CAL_FACTOR          1.02f

#define HX711_DOUT_PIN              GPIO_NUM_26
#define HX711_SCK_PIN               GPIO_NUM_27
#define HX711_KNOWN_WEIGHT_G        500.0f

#define VL53L0X_ADDRESS             0x29
#define VL53L0X_CAL_FACTOR          1.05f

// AGREGADO: Instancia global del nivómetro para tasks.c
nivometro_t g_nivometro;

// Inicializar I2C
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) return ret;
    
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void app_main(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "🏔️  Iniciando TFG Nivómetro Antártida");
    ESP_LOGI(TAG, "📊 Versión completa: LOGS + MQTT + API mejorada");
    
    // Inicializar NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Inicializar I2C para VL53L0X
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "✅ I2C inicializado");
    
    // CAMBIADO: Configurar nivómetro usando el componente nivometro_sensors
    nivometro_config_t nivometro_config = {
        .hcsr04p_trigger_pin = HCSR04P_TRIGGER_PIN,
        .hcsr04p_echo_pin = HCSR04P_ECHO_PIN,
        .hcsr04p_cal_factor = HCSR04P_CAL_FACTOR,
        
        .hx711_dout_pin = HX711_DOUT_PIN,
        .hx711_sck_pin = HX711_SCK_PIN,
        .hx711_gain = HX711_GAIN_128,
        .hx711_known_weight = HX711_KNOWN_WEIGHT_G,
        
        .vl53l0x_i2c_port = I2C_MASTER_NUM,
        .vl53l0x_address = VL53L0X_ADDRESS,
        .vl53l0x_accuracy = VL53L0X_ACCURACY_BETTER,
        .vl53l0x_cal_factor = VL53L0X_CAL_FACTOR
    };
    
    // Inicializar nivómetro
    ret = nivometro_init(&g_nivometro, &nivometro_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error inicializando nivómetro: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ Nivómetro inicializado correctamente");
    
    // Proceso de tara
    ESP_LOGI(TAG, "🔧 Iniciando proceso de tara automática...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ret = nivometro_tare_scale(&g_nivometro);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Tara completada exitosamente");
    } else {
        ESP_LOGW(TAG, "⚠️  Error en tara, continuando...");
    }
    
    ESP_LOGI(TAG, "🎉 Todos los sensores inicializados correctamente");
    ESP_LOGI(TAG, "📊 Configuración del sistema:");
    ESP_LOGI(TAG, "   HC-SR04P: Pines %d (trigger) y %d (echo)", HCSR04P_TRIGGER_PIN, HCSR04P_ECHO_PIN);
    ESP_LOGI(TAG, "   HX711: Pines %d (DOUT) y %d (SCK)", HX711_DOUT_PIN, HX711_SCK_PIN);
    ESP_LOGI(TAG, "   VL53L0X: I2C puerto %d, dirección 0x%02X", I2C_MASTER_NUM, VL53L0X_ADDRESS);
    
    // CAMBIADO: Usar tasks_start_all() en lugar de crear tareas manualmente
    tasks_start_all();
    
    ESP_LOGI(TAG, "🚀 Sistema nivómetro iniciado completamente");
    ESP_LOGI(TAG, "📈 Los datos se muestran en logs Y se envían por MQTT");
}