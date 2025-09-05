//File: components/communication/communication.c

#include "communication.h"
#include "sdkconfig.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "nivometro_sensors.h"
#include "esp_sntp.h"
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include "freertos/event_groups.h"

static const char* TAG = "communication";              // Etiqueta que usará esp_logx para clasificar mensajes de este módulo
static esp_mqtt_client_handle_t mqtt_client = NULL;    // Puntero al cliente mqtt una vez inicializado
static bool mqtt_started = false;                      // Indica si ya se ha arrancado el cliente mqtt
static bool mqtt_connected = false;                    // Variable para rastrear estado de conexión MQTT

// Grupo de eventos para coordinar estado wifi y mqtt
static EventGroupHandle_t comm_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;            // Bit que marca wifi listo
static const int MQTT_CONNECTED_BIT = BIT1;            // Bit que marca mqtt listo

// Topics mqtt donde se publicarán los datos
static const char* MQTT_TOPIC_ULTRASONIC = "sensors/ultrasonic";
static const char* MQTT_TOPIC_WEIGHT     = "sensors/weight";

// Prototipos de funciones internas
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
static void init_sntp_and_wait(void);                       // Arranca sntp y espera a sincronizar hora
static void get_iso8601_utc(char *out, size_t out_size);    // Rellena out con timestamp utc en formato iso8601

void communication_init(void) {
    // Crea el grupo de eventos para coordinar wifi y mqtt
    comm_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Inicializando módulo de comunicación...");

    // 1) Inicializar capa de red y sistema de eventos
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();                // Prepara interfaz wifi en modo estación (sta)

    // 2) Configurar driver wifi
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    // 3) Registrar handlers para eventos wifi/ip
    esp_event_handler_instance_t wifi_any_id_handle;
    esp_event_handler_instance_t ip_got_ip_handle;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_any_id_handle));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &ip_got_ip_handle));

    // 4) Preparar credenciales extraídas de sdkconfig
    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK
        }
    };

    // Copiar ssid y password del wifi desde sdkconfig
    strncpy((char*)wifi_cfg.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strncpy((char*)wifi_cfg.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));

    // 5) Arrancar wifi en modo sta con esa configuración
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 6) Bloquear aquí hasta que el handler marque WIFI_CONNECTED_BIT
    xEventGroupWaitBits(comm_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    // 7) Una vez con wifi, sincronizar hora con sntp
    init_sntp_and_wait();

    // 8) Configurar y arrancar cliente MQTT usando URI de sdkconfig
    esp_mqtt_client_config_t mqtt_cfg = {
          .broker.address.uri = "mqtt://broker.hivemq.com:1883"
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    esp_mqtt_client_start(mqtt_client);
    mqtt_started = true;
}

void communication_wait_for_connection(void) {
    // Espera hasta que tanto wifi como mqtt estén listos
    xEventGroupWaitBits(comm_event_group, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    // Lógica según evento wifi/ip
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Al arrancar wifi, disparar conexión
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // Si se desconecta: limpiar bits y reintentar
        xEventGroupClearBits(comm_event_group, WIFI_CONNECTED_BIT | MQTT_CONNECTED_BIT);
        mqtt_started = false;
        mqtt_connected = false;
        esp_wifi_connect();

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Cuando obtiene ip, marcar wifi listo
        xEventGroupSetBits(comm_event_group, WIFI_CONNECTED_BIT);
    }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    // Lógica para eventos del cliente mqtt
    if (event_id == MQTT_EVENT_CONNECTED) {
        // Se conectó al broker -> marcar mqtt listo
        mqtt_connected = true;
        xEventGroupSetBits(comm_event_group, MQTT_CONNECTED_BIT);
        ESP_LOGI(TAG, "MQTT conectado al broker");

    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        // Desconexión del broker -> limpiar bit y marcar estado
        mqtt_connected = false;
        xEventGroupClearBits(comm_event_group, MQTT_CONNECTED_BIT);
        mqtt_started = false;
        ESP_LOGW(TAG, "MQTT desconectado del broker");
        
    } else if (event_id == MQTT_EVENT_PUBLISHED) {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
        ESP_LOGD(TAG, "Mensaje MQTT enviado - msg_id: %d", event->msg_id);
        
    } else if (event_id == MQTT_EVENT_ERROR) {
        ESP_LOGE(TAG, "Error en evento MQTT");
    }
}

static void init_sntp_and_wait(void) {
    // Configura sntp para obtener la hora
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    // Espera en bucle hasta que se sincronice
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void get_iso8601_utc(char *out, size_t out_size) {
    // Obtiene tiempo actual y lo formatea en iso8601 utc
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm_utc;
    gmtime_r(&tv.tv_sec, &tm_utc);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

bool communication_is_mqtt_connected(void) {
    return mqtt_connected;
}

void communication_publish(const sensor_data_t* data) {
    // Protege contra llamadas inválidas
    if (!mqtt_client || !data) return;

    char ts[32], msg[128];
    // Timestamp en utc
    get_iso8601_utc(ts, sizeof(ts));

    // Publicar valor del sensor de ultrasonidos
    snprintf(msg, sizeof(msg), "{\"value\": %.2f, \"timestamp\": \"%s\"}", data->distance_cm, ts); 
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_ULTRASONIC, msg, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado en %s: %s", MQTT_TOPIC_ULTRASONIC, msg);

    // Publicar valor del sensor de peso
    snprintf(msg, sizeof(msg), "{\"value\": %.2f, \"timestamp\": \"%s\"}", data->weight_kg, ts);
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_WEIGHT, msg, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado en %s: %s", MQTT_TOPIC_WEIGHT, msg);
}