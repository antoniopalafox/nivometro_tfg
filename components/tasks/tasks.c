// tasks.c

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
}
