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
#define SENSOR_PERIOD_MS     60000                      // Intervalo de lectura: 60 000 ms (1 min)

// Parámetros de la tarea de publicación
#define PUBLISH_TASK_STACK   4096
#define PUBLISH_TASK_PRI     (tskIDLE_PRIORITY + 1)

static void sensor_task(void* _) {
    sensor_data_t d;
    nivometro_data_t nivometro_data;
    
    for (;;) {
        // CORREGIDO: Usar la API del nivómetro en lugar de sensors_read_all()
        esp_err_t result = nivometro_read_all_sensors(&g_nivometro, &nivometro_data);
        
        if (result == ESP_OK) {
            // Convertir nivometro_data_t a sensor_data_t
            nivometro_data_to_sensor_data(&nivometro_data, &d);
            
            if (xQueueSend(data_queue, &d, 0) != pdTRUE) {
                ESP_LOGW(TAG, "sensor_task: queue full, dropping sample");
            }
            ESP_LOGI(TAG, "Read: %.2f cm, %.2f kg, %.2f mm",
                     d.distance_cm, d.weight_kg, d.laser_mm);
        } else {
            ESP_LOGE(TAG, "Error reading sensors: %s", esp_err_to_name(result));
        }
        
        vTaskDelay(pdMS_TO_TICKS(SENSOR_PERIOD_MS));    // Espera el siguiente ciclo
    }
}

static void publish_task(void* _) {
    sensor_data_t d;

    for (;;) {
        // Bloquea hasta recibir un dato de sensor
        if (xQueueReceive(data_queue, &d, portMAX_DELAY) == pdTRUE) {
            storage_buffer_data(&d);               // Guardar localmente en nvs
            communication_wait_for_connection();   // Asegurar conexión wifi + mqtt
            communication_publish(&d);             // Enviar los datos al broker
            vTaskDelay(pdMS_TO_TICKS(200));        // Pausa tras publicar
            ESP_LOGI(TAG, "Entering deep sleep for 30 seconds");
            power_manager_enter_deep_sleep();      // Poner el esp32 en deep sleep
        }
    }
}

void tasks_start_all(void) {
    // Crear la cola con capacidad para 10 muestras de sensor_data_t
    data_queue = xQueueCreate(10, sizeof(sensor_data_t));
    if (!data_queue) {
        ESP_LOGE(TAG, "tasks_start_all: failed to create queue");
        return;
    }

    // Lanzar la tarea de lectura de sensores
    xTaskCreate(sensor_task, "sensor_task", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRI, NULL);

    // Lanzar la tarea de publicación y gestión de energía
    xTaskCreate(publish_task, "publish_task", PUBLISH_TASK_STACK, NULL, PUBLISH_TASK_PRI, NULL);

    ESP_LOGI(TAG, "tasks_start_all: all tasks started");
}