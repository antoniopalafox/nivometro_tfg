<<<<<<< HEAD
/*
=======
// tasks.c

>>>>>>> 309a69d84d31e1a8cbc061ce05c03b7c118ea376
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

static const char* TAG = "tasks";        // Etiqueta de logs para este módulo
static QueueHandle_t data_queue;         // Cola para pasar datos del sensor a la tarea de publicación

extern nivometro_t g_nivometro;          // Instancia global del nivómetro

// Parámetros de la tarea de lectura de sensores
#define SENSOR_TASK_STACK         2048
#define SENSOR_TASK_PRI           (tskIDLE_PRIORITY + 2)
#define SENSOR_PERIOD_NORMAL_MS   60000   // 1 min en modo normal
#define SENSOR_PERIOD_LOWPOWER_MS 300000  // 5 min en modo batería

// Parámetros de la tarea de publicación
#define PUBLISH_TASK_STACK            4096
#define PUBLISH_TASK_PRI              (tskIDLE_PRIORITY + 1)
#define PUBLISH_SHORT_DELAY_MS        200
#define PUBLISH_INTERVAL_NORMAL_MS    30000    // 30 s en modo normal

static void sensor_task(void* _) {
    sensor_data_t d;
    nivometro_data_t nivometro_data;

    for (;;) {
        // Leer sensores
        esp_err_t result = nivometro_read_all_sensors(&g_nivometro, &nivometro_data);
        if (result == ESP_OK) {
            nivometro_data_to_sensor_data(&nivometro_data, &d);
            if (xQueueSend(data_queue, &d, 0) != pdTRUE) {
                ESP_LOGW(TAG, "sensor_task: queue full, dropping sample");
            }
            ESP_LOGI(TAG, "Read: %.2f cm, %.2f kg, %.2f mm",
                     d.distance_cm, d.weight_kg, d.laser_mm);
        } else {
            ESP_LOGE(TAG, "Error reading sensors: %s", esp_err_to_name(result));
        }

        // Ajustar periodo según fuente de alimentación
        power_source_t src = power_manager_get_source();
        TickType_t delay_ms = (src == POWER_SOURCE_USB)
                              ? SENSOR_PERIOD_NORMAL_MS
                              : SENSOR_PERIOD_LOWPOWER_MS;
        ESP_LOGI(TAG, "sensor_task: next read in %d ms (%s)",
                 (int)delay_ms,
                 src == POWER_SOURCE_USB ? "normal" : "low power");
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void publish_task(void* _) {
    sensor_data_t d;

    for (;;) {
        // Espera a tener datos de sensor
        if (xQueueReceive(data_queue, &d, portMAX_DELAY) == pdTRUE) {
            storage_buffer_data(&d);               // Guardar localmente en NVS
            communication_wait_for_connection();   // Asegurar Wi‑Fi + MQTT
            communication_publish(&d);             // Enviar al broker
            vTaskDelay(pdMS_TO_TICKS(PUBLISH_SHORT_DELAY_MS));

            // Decidir siguiente acción según fuente de alimentación
            power_source_t src = power_manager_get_source();
            if (src == POWER_SOURCE_USB) {
                // En modo normal, esperamos un intervalo y continuamos
                ESP_LOGI(TAG, "USB power: delaying %d ms before next cycle",
                         PUBLISH_INTERVAL_NORMAL_MS);
                vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_NORMAL_MS));
            } else {
                // En modo batería, entramos en deep sleep
                ESP_LOGI(TAG, "Battery power: entering deep sleep");
                power_manager_enter_deep_sleep();
            }
        }
    }
}

void tasks_start_all(void) {
    data_queue = xQueueCreate(10, sizeof(sensor_data_t));
    if (!data_queue) {
        ESP_LOGE(TAG, "tasks_start_all: failed to create queue");
        return;
    }

    xTaskCreate(sensor_task,
                "sensor_task",
                SENSOR_TASK_STACK,
                NULL,
                SENSOR_TASK_PRI,
                NULL);

    xTaskCreate(publish_task,
                "publish_task",
                PUBLISH_TASK_STACK,
                NULL,
                PUBLISH_TASK_PRI,
                NULL);

    ESP_LOGI(TAG, "tasks_start_all: all tasks started");
<<<<<<< HEAD
}*/

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
#define SENSOR_TASK_STACK    2048
#define SENSOR_TASK_PRI      (tskIDLE_PRIORITY + 2)

// Intervalos de medición según fuente de alimentación
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
    
    ESP_LOGI(TAG, "📊 Tarea de sensores iniciada con gestión inteligente de energía");
    
    for (;;) {
        measurement_count++;
        
        // ⚡ DETECTAR FUENTE DE ALIMENTACIÓN ⚡
        power_source_t power_source = power_manager_get_source();
        uint32_t delay_ms;
        const char* mode_str;
        
        if (power_source == POWER_SOURCE_USB) {
            // 🔌 MODO USB: Mediciones frecuentes para máximo monitoreo
            delay_ms = SENSOR_PERIOD_USB_MS;
            mode_str = "USB-FRECUENTE";
            
            ESP_LOGI(TAG, "🔌 [%s] Medición completa #%lu (cada 5s)", mode_str, measurement_count);
            
        } else if (power_source == POWER_SOURCE_BATTERY) {
            // 🔋 MODO BATERÍA: Mediciones espaciadas para máxima duración
            delay_ms = SENSOR_PERIOD_BATTERY_MS;
            mode_str = "BATERÍA-ESPACIADO";
            
            ESP_LOGI(TAG, "🔋 [%s] Medición ahorro #%lu (cada 60s + sleep)", mode_str, measurement_count);
            
        } else {
            // ❓ MODO DESCONOCIDO: Usar configuración intermedia
            delay_ms = SENSOR_PERIOD_DEFAULT_MS;
            mode_str = "DESCONOCIDO";
            
            ESP_LOGW(TAG, "❓ [%s] Fuente desconocida, usando modo intermedio", mode_str);
        }
        
        // === LECTURA DE SENSORES (IGUAL PARA TODOS LOS MODOS) ===
        esp_err_t result = nivometro_read_all_sensors(&g_nivometro, &nivometro_data);
        
        if (result == ESP_OK) {
            // Convertir nivometro_data_t a sensor_data_t
            nivometro_data_to_sensor_data(&nivometro_data, &d);
            
            // Enviar datos a la cola para procesamiento
            if (xQueueSend(data_queue, &d, 0) != pdTRUE) {
                ESP_LOGW(TAG, "⚠️  [%s] Cola llena, descartando muestra", mode_str);
            } else {
                ESP_LOGI(TAG, "✅ [%s] Datos: %.2f cm, %.2f kg, %.2f mm", 
                        mode_str, d.distance_cm, d.weight_kg, d.laser_mm);
            }
        } else {
            ESP_LOGE(TAG, "❌ Error leyendo sensores: %s", esp_err_to_name(result));
        }
        
        // === LOGGING PERIÓDICO DE ESTADO ===
        if (measurement_count % 10 == 0) {
            ESP_LOGI(TAG, "📈 Estado del sistema - Medición: %lu, Modo: %s, Intervalo: %lu ms", 
                    measurement_count, mode_str, delay_ms);
        }
        
        // Esperar según el modo de alimentación
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/**
 * Tarea de publicación con gestión inteligente de energía
 */
static void publish_task(void* _) {
    sensor_data_t d;
    uint32_t publish_count = 0;

    ESP_LOGI(TAG, "📡 Tarea de publicación iniciada con gestión inteligente de energía");

    for (;;) {
        // Bloquea hasta recibir un dato de sensor
        if (xQueueReceive(data_queue, &d, portMAX_DELAY) == pdTRUE) {
            publish_count++;
            
            // ⚡ DETECTAR FUENTE DE ALIMENTACIÓN ⚡
            power_source_t power_source = power_manager_get_source();
            
            // === ALMACENAMIENTO LOCAL (SIEMPRE) ===
            storage_buffer_data(&d);
            ESP_LOGI(TAG, "💾 Datos guardados localmente");
            
            if (power_source == POWER_SOURCE_USB) {
                // ═══════════════════════════════════════
                // 🔌 MODO USB: COMUNICACIÓN COMPLETA
                // ═══════════════════════════════════════
                
                ESP_LOGI(TAG, "🔌 [USB-FRECUENTE] Publicación completa #%lu", publish_count);
                
                // Asegurar conexión WiFi + MQTT
                communication_wait_for_connection();
                
                // Enviar datos al broker
                communication_publish(&d);
                ESP_LOGI(TAG, "✅ [USB-FRECUENTE] Datos enviados vía MQTT");
                
                // Pausa breve y continuar (NO deep sleep)
                vTaskDelay(pdMS_TO_TICKS(500));
                ESP_LOGI(TAG, "🔌 [USB-FRECUENTE] Continuando en modo activo...");
                
            } else if (power_source == POWER_SOURCE_BATTERY) {
                // ═══════════════════════════════════════
                // 🔋 MODO BATERÍA: AHORRO MÁXIMO
                // ═══════════════════════════════════════
                
                ESP_LOGI(TAG, "🔋 [BATERÍA-ESPACIADO] Publicación #%lu", publish_count);
                
                // Intentar envío rápido si hay conexión
                if (communication_is_connected()) {
                    ESP_LOGI(TAG, "📡 Conexión disponible - envío rápido");
                    communication_publish(&d);
                    ESP_LOGI(TAG, "✅ [BATERÍA-ESPACIADO] Datos enviados");
                } else {
                    ESP_LOGI(TAG, "📶 Sin conexión - datos solo locales");
                }
                
                // Verificar si debe entrar en deep sleep
                if (power_manager_should_sleep()) {
                    ESP_LOGI(TAG, "💤 [BATERÍA-ESPACIADO] Condiciones para sleep cumplidas");
                    
                    // Desconectar comunicaciones para ahorrar energía
                    // communication_disconnect(); // Si tienes esta función
                    
                    ESP_LOGI(TAG, "😴 Entrando en deep sleep por 30 segundos...");
                    power_manager_enter_deep_sleep();
                    
                    // ⚠️ EL SISTEMA SE REINICIA AQUÍ ⚠️
                }
                
                // Si no entra en sleep, pausa breve
                vTaskDelay(pdMS_TO_TICKS(200));
                
            } else {
                // ═══════════════════════════════════════
                // ❓ MODO DESCONOCIDO: COMPORTAMIENTO CONSERVATIVO
                // ═══════════════════════════════════════
                
                ESP_LOGW(TAG, "❓ [DESCONOCIDO] Fuente desconocida - modo conservativo");
                
                // Comportamiento conservativo: intentar envío pero prepararse para sleep
                if (communication_is_connected()) {
                    communication_publish(&d);
                    ESP_LOGI(TAG, "✅ [DESCONOCIDO] Datos enviados (modo conservativo)");
                }
                
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            // === LOGGING PERIÓDICO ===
            if (publish_count % 5 == 0) {
                const char* source_str = (power_source == POWER_SOURCE_USB) ? "USB" : 
                                        (power_source == POWER_SOURCE_BATTERY) ? "BATERÍA" : "DESCONOCIDO";
                ESP_LOGI(TAG, "📊 Resumen publicaciones: %lu completadas, Fuente: %s", 
                        publish_count, source_str);
            }
        }
    }
}

void tasks_start_all(void) {
    ESP_LOGI(TAG, "🚀 Iniciando todas las tareas con gestión inteligente de energía...");
    
    // Crear la cola con capacidad para 10 muestras de sensor_data_t
    data_queue = xQueueCreate(10, sizeof(sensor_data_t));
    if (!data_queue) {
        ESP_LOGE(TAG, "❌ Error: No se pudo crear la cola de datos");
        return;
    }
    ESP_LOGI(TAG, "✅ Cola de datos creada (capacidad: 10 muestras)");

    // Lanzar la tarea de lectura de sensores
    BaseType_t sensor_result = xTaskCreate(
        sensor_task, 
        "sensor_task", 
        SENSOR_TASK_STACK, 
        NULL, 
        SENSOR_TASK_PRI, 
        NULL
    );
    
    if (sensor_result == pdPASS) {
        ESP_LOGI(TAG, "✅ Tarea de sensores creada exitosamente");
    } else {
        ESP_LOGE(TAG, "❌ Error creando tarea de sensores");
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
        ESP_LOGI(TAG, "✅ Tarea de publicación creada exitosamente");
    } else {
        ESP_LOGE(TAG, "❌ Error creando tarea de publicación");
        return;
    }

    ESP_LOGI(TAG, "🎉 Todas las tareas iniciadas correctamente");
    ESP_LOGI(TAG, "⚡ Gestión automática de energía activa:");
    ESP_LOGI(TAG, "   🔌 USB conectado → Mediciones frecuentes (cada 5s, sin sleep)");
    ESP_LOGI(TAG, "   🔋 Solo batería → Mediciones espaciadas (cada 60s + deep sleep 30s)");
=======
>>>>>>> 309a69d84d31e1a8cbc061ce05c03b7c118ea376
}
