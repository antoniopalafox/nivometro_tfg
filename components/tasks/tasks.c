//File: components/tasks/tasks.c
#include "tasks.h"
#include "nivometro_sensors.h"
#include "storage.h"
#include "communication.h"
#include "power_manager.h"
#include "utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <stdbool.h>

static const char* TAG = "tasks";                       // Etiqueta de logs para este módulo
static QueueHandle_t data_queue;                        // Cola para pasar datos del sensor a la tarea de publicación

// AGREGADO: Instancia global del nivómetro (debe ser inicializada desde main)
extern nivometro_t g_nivometro;

// Parámetros de la tarea de lectura de sensores
#define SENSOR_TASK_STACK    4096                       // AUMENTADO: Stack mayor para evitar overflow
#define SENSOR_TASK_PRI      (tskIDLE_PRIORITY + 2)

// ⚡ INTERVALOS CORREGIDOS - ESTOS SON LOS VALORES REALES QUE SE USAN
#define SENSOR_PERIOD_USB_MS      5000                  // USB: 5 segundos (mediciones frecuentes)
#define SENSOR_PERIOD_BATTERY_MS  60000                 // Batería: 60 segundos (máximo ahorro)
#define SENSOR_PERIOD_DEFAULT_MS  30000                 // Default: 30 segundos

// Parámetros de la tarea de publicación
#define PUBLISH_TASK_STACK   4096
#define PUBLISH_TASK_PRI     (tskIDLE_PRIORITY + 1)

/**
 * Tarea de lectura de sensores con gestión inteligente de energía 
 */
static void sensor_task(void* _) {
    sensor_data_t d;
    nivometro_data_t nivometro_data;
    uint32_t measurement_count = 0;
    
        
    for (;;) {
        measurement_count++;
        
        // DETECTAR FUENTE DE ALIMENTACIÓN ANTES DE CADA CICLO ⚡
        power_source_t power_source = power_manager_get_source();
        uint32_t delay_ms;
        const char* mode_str;
        
        // === DETERMINACIÓN DE INTERVALO SEGÚN FUENTE ===
        if (power_source == POWER_SOURCE_USB) {
            // 🔌 MODO USB: Mediciones frecuentes cada 5 segundos
            delay_ms = SENSOR_PERIOD_USB_MS;  // 5000 ms
            mode_str = "USB-FRECUENTE";
            
            ESP_LOGI(TAG, "[%s] Medición #%lu - Intervalo: %lu ms (5 segundos)", 
                    mode_str, measurement_count, delay_ms);
            
        } else if (power_source == POWER_SOURCE_BATTERY) {
            // MODO BATERÍA: Mediciones espaciadas cada 60 segundos
            delay_ms = SENSOR_PERIOD_BATTERY_MS;  // 60000 ms
            mode_str = "BATERÍA-ESPACIADO";
            
            ESP_LOGI(TAG, "[%s] Medición #%lu - Intervalo: %lu ms (60 segundos + sleep)", 
                    mode_str, measurement_count, delay_ms);
            
        } else {
            // MODO DESCONOCIDO: Usar configuración intermedia
            delay_ms = SENSOR_PERIOD_DEFAULT_MS;  // 30000 ms
            mode_str = "DESCONOCIDO";
            
            ESP_LOGW(TAG, "[%s] Medición #%lu - Intervalo: %lu ms (modo intermedio)", 
                    mode_str, measurement_count, delay_ms);
        }
        
        // === LECTURA DE SENSORES ===
        esp_err_t result = nivometro_read_all_sensors(&g_nivometro, &nivometro_data);
        
        if (result == ESP_OK) {
            // Convertir nivometro_data_t a sensor_data_t
            nivometro_data_to_sensor_data(&nivometro_data, &d);
            
            // Enviar datos a la cola para procesamiento
            if (xQueueSend(data_queue, &d, 0) != pdTRUE) {
                ESP_LOGW(TAG, "[%s] Cola llena, descartando muestra", mode_str);
            } else {
                ESP_LOGI(TAG, "[%s] Datos enviados: %.2f cm, %.3f kg", 
                        mode_str, d.distance_cm, d.weight_kg);
            }
        } else {
            ESP_LOGE(TAG, "Error leyendo sensores: %s", esp_err_to_name(result));
        }
        
        // === LOGGING DE CONFIRMACIÓN DEL INTERVALO ===
        ESP_LOGI(TAG, "[%s] Esperando %lu ms antes de la siguiente medición", mode_str, delay_ms);
        
        // === ESPERAR EL TIEMPO DETERMINADO ===
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// Tarea de publicación con gestión inteligente de energía
static void publish_task(void* _) {
    sensor_data_t d;
    uint32_t publish_count = 0;

    for (;;) {
        // Bloquea hasta recibir un dato de sensor
        if (xQueueReceive(data_queue, &d, portMAX_DELAY) == pdTRUE) {
            publish_count++;
            
            // DETECTAR FUENTE DE ALIMENTACIÓN 
            power_source_t power_source = power_manager_get_source();
            
            // === ALMACENAMIENTO LOCAL (SIEMPRE) ===
            storage_buffer_data(&d);
            ESP_LOGI(TAG, "[Pub #%lu] Datos guardados localmente", publish_count);
            
            if (power_source == POWER_SOURCE_USB) {
                // ═══════════════════════════════════════
                // MODO USB: COMUNICACIÓN COMPLETA
                // ═══════════════════════════════════════
                
                ESP_LOGI(TAG, "[USB-Conectado] Publicación #%lu - Modo nominal", publish_count);
                
                // Asegurar conexión WiFi + MQTT
                communication_wait_for_connection();
                
                // Enviar datos al broker
                communication_publish(&d);
                ESP_LOGI(TAG, "[USB-Conectado] Datos enviados vía MQTT");
                
                // Pausa breve y continuar (NO deep sleep)
                vTaskDelay(pdMS_TO_TICKS(500));
                ESP_LOGI(TAG, "[USB-Conectado] Continuando en modo nominal");
                
            } else if (power_source == POWER_SOURCE_BATTERY) {
                // ═══════════════════════════════════════
                // MODO BATERÍA: AHORRO MÁXIMO CON VERIFICACIÓN
                // ═══════════════════════════════════════
                
                ESP_LOGI(TAG, "[Batería] Publicación #%lu - Modo Batería", publish_count);
                
                // Verificar peso válido
                if (d.weight_kg == 0.0f) {
                    ESP_LOGW(TAG, "[Batería] PESO CERO detectado - Verificar HX711");
                }
                
                // Verificar conexión MQTT con timeout
                ESP_LOGI(TAG, "[Batería] Verificando conexión MQTT...");
                
                uint32_t wait_start = xTaskGetTickCount();
                uint32_t max_wait_ticks = pdMS_TO_TICKS(10000); // 10 segundos
                
                while (!communication_is_mqtt_connected() && 
                       (xTaskGetTickCount() - wait_start) < max_wait_ticks) {
                    ESP_LOGD(TAG, "[Batería] Esperando conexión MQTT...");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                
                if (communication_is_mqtt_connected()) {
                    ESP_LOGI(TAG, "[Batería] MQTT conectado - Enviando datos");
                    communication_publish(&d);
                    ESP_LOGI(TAG, "[Batería] Datos enviados");
                    
                    // Dar tiempo para confirmación
                    vTaskDelay(pdMS_TO_TICKS(3000));
                } else {
                    ESP_LOGW(TAG, "[Batería] MQTT no conectado - Enviando de todas formas");
                    communication_publish(&d);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
                
                // Verificar si debe entrar en deep sleep
                if (power_manager_should_sleep()) {
                    ESP_LOGI(TAG, "[Batería] Condiciones para modo batería cumplidas");
                    ESP_LOGI(TAG, "[Batería] Esperando 2 segundos antes de deep sleep...");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    
                    ESP_LOGI(TAG, "[Batería] Entrando en deep_sleep...");
                    power_manager_enter_deep_sleep();
                    
                    // EL SISTEMA SE REINICIA AQUÍ 
                }
                
                vTaskDelay(pdMS_TO_TICKS(200));
                
            } else {
                // ═══════════════════════════════════════
                // MODO DESCONOCIDO: COMPORTAMIENTO CONSERVATIVO
                // ═══════════════════════════════════════
                
                ESP_LOGW(TAG, "[DESCONOCIDO] Publicación #%lu - modo conservativo", publish_count);
                
                communication_publish(&d);
                ESP_LOGI(TAG, "[DESCONOCIDO] Datos enviados (modo conservativo)");
                
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}

void tasks_start_all(void) {
    ESP_LOGI(TAG, "Iniciando todas las tareas con gestión inteligente de energía");
    ESP_LOGI(TAG, "Intervalos configurados:");
    ESP_LOGI(TAG, "Nominal: %d ms (%d segundos)", SENSOR_PERIOD_USB_MS, SENSOR_PERIOD_USB_MS/1000);
    ESP_LOGI(TAG, "Batería: %d ms (%d segundos)", SENSOR_PERIOD_BATTERY_MS, SENSOR_PERIOD_BATTERY_MS/1000);
    ESP_LOGI(TAG, "Default: %d ms (%d segundos)", SENSOR_PERIOD_DEFAULT_MS, SENSOR_PERIOD_DEFAULT_MS/1000);
    
    // Crear la cola con capacidad para 10 muestras de sensor_data_t
    data_queue = xQueueCreate(10, sizeof(sensor_data_t));
    if (!data_queue) {
        ESP_LOGE(TAG, "Error: No se pudo crear la cola de datos");
        return;
    }
    ESP_LOGI(TAG, "Cola de datos creada (capacidad: 10 muestras)");

    // Lanzar la tarea de lectura de sensores con stack aumentado
    BaseType_t sensor_result = xTaskCreate(
        sensor_task, 
        "sensor_task", 
        SENSOR_TASK_STACK,  // 4096 bytes (aumentado)
        NULL, 
        SENSOR_TASK_PRI, 
        NULL
    );
    
    if (sensor_result == pdPASS) {
        ESP_LOGI(TAG, "Tarea de sensores creada con stack de %d bytes", SENSOR_TASK_STACK);
    } else {
        ESP_LOGE(TAG, "Error creando tarea de sensores");
        return;
    }

    // Lanzar la tarea de publicación y gestión de energía
    BaseType_t publish_result = xTaskCreate(
        publish_task, 
        "publish_task", 
        PUBLISH_TASK_STACK, 
        NULL, 
        PUBLISH_TASK_PRI, 
        NULL
    );
    
    if (publish_result == pdPASS) {
        ESP_LOGI(TAG, "Tarea de publicación creada exitosamente");
    } else {
        ESP_LOGE(TAG, "Error creando tarea de publicación");
        return;
    }

    ESP_LOGI(TAG, "Todas las tareas iniciadas correctamente");
    ESP_LOGI(TAG, "Gestión automática de energía activa:");
    ESP_LOGI(TAG, "USB conectado → Mediciones cada 5 segundos, modo nominal");
    ESP_LOGI(TAG, "Solo batería → Mediciones cada 60 segundos + modo batería");
}