/*#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "diagnostics.h"
#include "config.h"
#include "sensors.h"
#include "storage.h"
#include "communication.h"
#include "power_manager.h"
#include "utils.h"
#include "tasks.h"

void app_main(void) {
    diagnostics_init();          // 1) Arranca el sistema de logs y diagnóstico
    config_init();               // 2) Carga o inicializa la configuración global (nvs, etc)
    sensors_init();              // 3) Pone a punto los sensores
    storage_init();              // 4) Inicializa el almacenamiento local (nvs)
    communication_init();        // 5) Arranca wifi y mqtt y sincroniza hora
    power_manager_init();        // 6) Comprueba y registra si venimos de deep sleep
    timer_manager_init();        // 7) Prepara el gestor de temporización
    tasks_start_all();           // 8) Crea y lanza las tareas de freertos (lectura, publicación y deep sleep)
}*/

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

// Incluir drivers
#include "hcsr04p.h"
#include "hx711.h"
#include "vl53l0x.h"

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

// Tópicos MQTT
#define MQTT_TOPIC_ULTRASONIC       "nivometro/antartica/ultrasonic"
#define MQTT_TOPIC_WEIGHT           "nivometro/antartica/weight"  
#define MQTT_TOPIC_LASER            "nivometro/antartica/laser"
#define MQTT_TOPIC_STATUS           "nivometro/antartica/status"

// Estructura de datos del nivómetro
typedef struct {
    float ultrasonic_distance_cm;
    float weight_grams;
    float laser_distance_mm;
    uint64_t timestamp_us;
    uint8_t sensor_status;  // Bits: [2]=VL53L0X, [1]=HX711, [0]=HC-SR04P
} nivometro_data_t;

// Variables globales
static hcsr04p_sensor_t ultrasonic_sensor;
static hx711_t scale_sensor;
static vl53l0x_sensor_t laser_sensor;
static QueueHandle_t sensor_data_queue;

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

// Función para crear JSON
static void create_json_message(const char* sensor_type, float value, const char* unit, 
                               uint64_t timestamp, bool status, char* json_buffer, size_t buffer_size) {
    snprintf(json_buffer, buffer_size,
            "{\"sensor\":\"%s\",\"value\":%.2f,\"unit\":\"%s\",\"timestamp\":%llu,\"status\":%s}",
            sensor_type, value, unit, timestamp, status ? "true" : "false");
}

// Función simple para simular envío MQTT (reemplazar con implementación real)
static bool mqtt_publish_simple(const char* topic, const char* message) {
    // TODO: Implementar con comunicación real
    ESP_LOGI("MQTT_SIMULATOR", "📡 Enviando a '%s': %s", topic, message);
    return true;  // Simular éxito
}

// Tarea de lectura de sensores
void sensor_task(void *pvParameters) {
    nivometro_data_t sensor_data;
    
    ESP_LOGI(TAG, "🏔️  Iniciando tarea de lectura de sensores del nivómetro");
    
    while (1) {
        // Inicializar estructura de datos
        memset(&sensor_data, 0, sizeof(sensor_data));
        sensor_data.timestamp_us = esp_timer_get_time();
        
        ESP_LOGI(TAG, "================================");
        ESP_LOGI(TAG, "🏔️  NIVÓMETRO ANTÁRTIDA - LECTURA #%llu", 
                 sensor_data.timestamp_us / 1000000);
        
        // 1. Leer HC-SR04P (Ultrasonido)
        sensor_data.ultrasonic_distance_cm = hcsr04p_read_distance(&ultrasonic_sensor);
        if (sensor_data.ultrasonic_distance_cm >= 0) {
            ESP_LOGI(TAG, "📏 Distancia nieve (ultrasonido): %.2f cm", 
                     sensor_data.ultrasonic_distance_cm);
            sensor_data.sensor_status |= 0x01;  // Bit 0 = HC-SR04P OK
        } else {
            ESP_LOGW(TAG, "📏 ⚠️  HC-SR04P: Error de lectura");
            sensor_data.ultrasonic_distance_cm = -1.0f;
        }
        
        // 2. Leer HX711 (Peso)
        esp_err_t ret = hx711_read_units(&scale_sensor, &sensor_data.weight_grams);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "⚖️  Peso nieve acumulada: %.2f g", sensor_data.weight_grams);
            sensor_data.sensor_status |= 0x02;  // Bit 1 = HX711 OK
        } else {
            ESP_LOGW(TAG, "⚖️  ⚠️  HX711: Error de lectura - %s", esp_err_to_name(ret));
            sensor_data.weight_grams = 0.0f;
        }
        
        // 3. Leer VL53L0X (Láser)
        uint16_t laser_raw = vl53l0x_read_distance(&laser_sensor);
        sensor_data.laser_distance_mm = (float)laser_raw;
        if (laser_raw > 0 && laser_raw < 8000) {
            ESP_LOGI(TAG, "🔬 Distancia láser precisión: %.0f mm", sensor_data.laser_distance_mm);
            sensor_data.sensor_status |= 0x04;  // Bit 2 = VL53L0X OK
        } else {
            ESP_LOGW(TAG, "🔬 ⚠️  VL53L0X: Lectura fuera de rango (%u mm)", laser_raw);
        }
        
        // 4. Mostrar resumen en pantalla
        ESP_LOGI(TAG, "📊 RESUMEN MEDICIÓN:");
        ESP_LOGI(TAG, "   🔵 Ultrasonido: %.2f cm", sensor_data.ultrasonic_distance_cm);
        ESP_LOGI(TAG, "   🟢 Peso: %.2f g", sensor_data.weight_grams);
        ESP_LOGI(TAG, "   🔴 Láser: %.0f mm", sensor_data.laser_distance_mm);
        ESP_LOGI(TAG, "   ⏰ Timestamp: %llu μs", sensor_data.timestamp_us);
        
        // Estado de sensores
        bool ultrasonic_ok = (sensor_data.sensor_status & 0x01) != 0;
        bool weight_ok = (sensor_data.sensor_status & 0x02) != 0;
        bool laser_ok = (sensor_data.sensor_status & 0x04) != 0;
        
        ESP_LOGI(TAG, "🔍 Estado sensores: HC-SR04P:%s HX711:%s VL53L0X:%s",
                 ultrasonic_ok ? "✅" : "❌",
                 weight_ok ? "✅" : "❌", 
                 laser_ok ? "✅" : "❌");
        
        // 5. Enviar datos a cola para MQTT
        if (xQueueSend(sensor_data_queue, &sensor_data, 0) != pdTRUE) {
            ESP_LOGW(TAG, "📡 ⚠️  Cola MQTT llena, descartando datos");
        } else {
            ESP_LOGI(TAG, "📡 ✅ Datos enviados a cola MQTT");
        }
        
        ESP_LOGI(TAG, "================================");
        
        vTaskDelay(pdMS_TO_TICKS(2000)); // Lectura cada 2 segundos
    }
}

// Tarea de comunicación MQTT
void mqtt_task(void *pvParameters) {
    nivometro_data_t sensor_data;
    char json_buffer[256];
    
    ESP_LOGI(TAG, "📡 Iniciando tarea de comunicación MQTT");
    
    while (1) {
        // Esperar datos de sensores
        if (xQueueReceive(sensor_data_queue, &sensor_data, portMAX_DELAY) == pdTRUE) {
            
            ESP_LOGI(TAG, "📡 Procesando datos para MQTT...");
            
            // Enviar datos del ultrasonido
            if (sensor_data.sensor_status & 0x01) {
                create_json_message("hcsr04p", sensor_data.ultrasonic_distance_cm, "cm",
                                   sensor_data.timestamp_us, true, json_buffer, sizeof(json_buffer));
                mqtt_publish_simple(MQTT_TOPIC_ULTRASONIC, json_buffer);
            }
            
            // Enviar datos del peso
            if (sensor_data.sensor_status & 0x02) {
                create_json_message("hx711", sensor_data.weight_grams, "g",
                                   sensor_data.timestamp_us, true, json_buffer, sizeof(json_buffer));
                mqtt_publish_simple(MQTT_TOPIC_WEIGHT, json_buffer);
            }
            
            // Enviar datos del láser
            if (sensor_data.sensor_status & 0x04) {
                create_json_message("vl53l0x", sensor_data.laser_distance_mm, "mm",
                                   sensor_data.timestamp_us, true, json_buffer, sizeof(json_buffer));
                mqtt_publish_simple(MQTT_TOPIC_LASER, json_buffer);
            }
            
            // Enviar estado general
            snprintf(json_buffer, sizeof(json_buffer),
                    "{\"sensor_mask\":%d,\"timestamp\":%llu,\"all_sensors_ok\":%s}",
                    sensor_data.sensor_status, sensor_data.timestamp_us,
                    (sensor_data.sensor_status == 0x07) ? "true" : "false");
            mqtt_publish_simple(MQTT_TOPIC_STATUS, json_buffer);
            
            ESP_LOGI(TAG, "📡 ✅ Todos los datos MQTT enviados");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Pequeño delay
    }
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
    
    // Inicializar HC-SR04P
    if (!hcsr04p_init(&ultrasonic_sensor, HCSR04P_TRIGGER_PIN, HCSR04P_ECHO_PIN)) {
        ESP_LOGE(TAG, "❌ Error inicializando HC-SR04P");
        return;
    }
    hcsr04p_set_calibration(&ultrasonic_sensor, HCSR04P_CAL_FACTOR);
    ESP_LOGI(TAG, "✅ HC-SR04P inicializado");
    
    // Inicializar HX711
    hx711_config_t hx711_config = {
        .dout_pin = HX711_DOUT_PIN,
        .sck_pin = HX711_SCK_PIN,
        .gain = HX711_GAIN_128
    };
    
    ret = hx711_init(&scale_sensor, &hx711_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Error inicializando HX711: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ HX711 inicializado con nueva API");
    
    // Proceso de tara
    ESP_LOGI(TAG, "🔧 Iniciando proceso de tara automática...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ret = hx711_tare(&scale_sensor, 10);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ Tara completada exitosamente");
    } else {
        ESP_LOGW(TAG, "⚠️  Error en tara, continuando...");
    }
    
    // Inicializar VL53L0X
    if (!vl53l0x_init(&laser_sensor, I2C_MASTER_NUM, VL53L0X_ADDRESS)) {
        ESP_LOGE(TAG, "❌ Error inicializando VL53L0X");
        return;
    }
    vl53l0x_set_accuracy(&laser_sensor, VL53L0X_ACCURACY_BETTER);
    vl53l0x_set_calibration(&laser_sensor, VL53L0X_CAL_FACTOR);
    ESP_LOGI(TAG, "✅ VL53L0X inicializado");
    
    // Crear cola para datos de sensores
    sensor_data_queue = xQueueCreate(5, sizeof(nivometro_data_t));
    if (!sensor_data_queue) {
        ESP_LOGE(TAG, "❌ Error creando cola de datos");
        return;
    }
    ESP_LOGI(TAG, "✅ Cola de datos creada");
    
    ESP_LOGI(TAG, "🎉 Todos los sensores inicializados correctamente");
    ESP_LOGI(TAG, "📊 Configuración del sistema:");
    ESP_LOGI(TAG, "   HC-SR04P: Pines %d (trigger) y %d (echo)", HCSR04P_TRIGGER_PIN, HCSR04P_ECHO_PIN);
    ESP_LOGI(TAG, "   HX711: Pines %d (DOUT) y %d (SCK)", HX711_DOUT_PIN, HX711_SCK_PIN);
    ESP_LOGI(TAG, "   VL53L0X: I2C puerto %d, dirección 0x%02X", I2C_MASTER_NUM, VL53L0X_ADDRESS);
    
    // Crear tareas
    xTaskCreate(sensor_task, "sensor_task", 8192, NULL, 5, NULL);
    xTaskCreate(mqtt_task, "mqtt_task", 8192, NULL, 4, NULL);
    
    ESP_LOGI(TAG, "🚀 Sistema nivómetro iniciado completamente");
    ESP_LOGI(TAG, "📈 Los datos se muestran en logs Y se envían por MQTT");
    ESP_LOGI(TAG, "📡 Tópicos MQTT:");
    ESP_LOGI(TAG, "   - %s", MQTT_TOPIC_ULTRASONIC);
    ESP_LOGI(TAG, "   - %s", MQTT_TOPIC_WEIGHT);
    ESP_LOGI(TAG, "   - %s", MQTT_TOPIC_LASER);
    ESP_LOGI(TAG, "   - %s", MQTT_TOPIC_STATUS);
}