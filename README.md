# 🏔️ TFG: Monitor de neutrones Estación Antártica Juan Carlos I – Proyecto Conjunto

**Antonio Mata Marco & Antonio Palafox Moya**  
Trabajo Fin de Grado en Ingeniería Telemática · Universidad de Alcalá

---

## Descripción

Se ha desarrollado una **estación IoT autónoma** para medir y transmitir en tiempo real la acumulación de nieve en la Antártida. El sistema combina:

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

### Instalación Rápida

1. **Clona el repositorio**  
   ```bash
   git clone https://github.com/antoniopalafox/nivometro_tfg.git
   cd nivometro_tfg
2. **Configurar credenciales Wi-Fi & MQTT**  
   ```bash
   idf.py menuconfig
   Serial Flasher Config → Flash Size: 4 MB
   Component Config → Communication configuration:
   Rellenar WiFi SSID, WiFi Password y MQTT Broker URI.
3. **Compilar**
    ```bash
    idf.py build
4. **Desplegar monitorización**
    ```bash
    cd tfg_telegraf_influx_grafana
    docker-compose up -d
    Grafana en: http://localhost:3000
    InfluxDB en: http:/localhost:8086 
5. **Ejecutar ESP32**
    ```bash
    cd ..
    idf.py flash monitor