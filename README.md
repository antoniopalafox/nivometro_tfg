# Nivómetro de neutrones Estación Antártica Juan Carlos I – Proyecto de Fin de Grado

**Antonio Mata Marco & Antonio Palafox Moya**  
Trabajo Fin de Grado en Ingeniería Telemática · Universidad de Alcalá

---

## Descripción

Se ha desarrollado una **estación IoT autónoma** para medir y transmitir en tiempo real la acumulación de nieve en la estación Antártica. El sistema combina:

- **HC-SR04P (ultrasonidos)**  
  Mide el espesor de la capa de nieve, diseñado para entornos extremos y resistente al agua.  
- **Celda de carga + HX711 (peso)**  
  Estima el peso acumulado, permitiendo corregir posibles sesgos de lectura por la compactación de la nieve.  
- **VL53L0X (láser)**  
  Obtiene distancias de alta precisión en rangos cortos para validar y afinar las lecturas ultrasónicas.

Todas las lecturas se publican vía **MQTT** en:

1. **Telegraf** – Agente que suscribe MQTT y envía datos a InfluxDB  
2. **InfluxDB** – Base de datos de series temporales  
3. **Grafana** – Dashboard web para visualización y análisis  

Gracias a la **gestión de la alimentación**, el dispositivo puede funcionar **en los modos nominal y de bajo consumo**

---

## Tecnologías

### Hardware
- **ESP32**: microcontrolador principal.  
- **HC-SR04P**: sensor de ultrasonidos.  
- **HX711**: sensor de peso.  
- **VL53L0X**: sensor sensor láser.

### Software
- **ESP-IDF v5** + **FreeRTOS** (framework ESP32).  
- **MQTT** (Mosquitto).  
- **Docker** & **Docker Compose** (containerización).  
- **Telegraf** (recolección de métricas MQTT→InfluxDB).  
- **InfluxDB** (almacenamiento de series temporales).  
- **Grafana** (visualización interactiva).

---

## Instalación y Uso

### Requisitos
- **ESP-IDF v5.0+**  
- **Docker** & **Docker Compose**  
- **Git**  
- **Python 3** (para ESP-IDF)

---

### Modo de uso

1. **Clonar el repositorio**  
   ```bash
   git clone https://github.com/antoniopalafox/nivometro_tfg.git
   cd nivometro_tfg
2. **Configuración credenciales Wi-Fi & MQTT**  
   ```bash
   idf.py menuconfig
   Serial Flasher Config → Flash Size: 4 MB
   Component Config → Communication configuration:
   Rellenar WiFi SSID, WiFi Password y MQTT Broker URI con la configuración deseada.
3. **Compilación**
    ```bash
    idf.py build
4. **Configurar variables de entorno**  
   Copia la plantilla y rellena tus credenciales reales:

    ```bash
    cd tfg_telegraf_influx_grafana
    cp .env.example .env
    # abre .env en tu editor y completa:
    # DOCKER_INFLUXDB_INIT_USERNAME=<tu_usuario_influx>
    # DOCKER_INFLUXDB_INIT_PASSWORD=<tu_password_influx>
    # DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=<tu_token_influx>
    # GF_ADMIN_USER=<tu_usuario_grafana>
    # GF_ADMIN_PASSWORD=<tu_password_grafana>
5. **Desplegar monitorización**  
    ```bash
    cd tfg_telegraf_influx_grafana
    docker-compose up -d
    ```
  Grafana en: http://localhost:3000

  InfluxDB en: http://localhost:8086
  
6. **Ejecutar ESP32**
   ```bash
   cd ..
   idf.py flash monitor