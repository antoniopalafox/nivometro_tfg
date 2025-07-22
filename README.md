# 🏔️ TFG Nivómetro Antártida - Proyecto Fusionado

Sistema completo de monitorización de nivel de nieve en la Antártida desarrollado como Trabajo Fin de Grado por Antonio Mata y Antonio Palafox.

## 🎯 Descripción

Sistema IoT basado en ESP32 que utiliza múltiples sensores para medir:
- **Distancia de nieve** (HC-SR04P ultrasonido)
- **Peso de nieve acumulada** (HX711 galga extensiométrica)  
- **Distancia de precisión** (VL53L0X láser ToF)

Los datos se envían vía MQTT a un stack de monitorización compuesto por Telegraf, InfluxDB y Grafana.

## 🛠️ Tecnologías

### Hardware
- **ESP32** (microcontrolador principal)
- **HC-SR04P** (sensor ultrasónico resistente al agua)
- **HX711** (amplificador para galga extensiométrica)
- **VL53L0X** (sensor láser ToF de alta precisión)

### Software
- **ESP-IDF v5** (framework profesional ESP32)
- **MQTT** (protocolo de comunicación IoT)
- **Docker** + **Docker Compose** (containerización)
- **Telegraf** (recolección de métricas)
- **InfluxDB** (base de datos temporal)
- **Grafana** (visualización de datos)

## 🚀 Instalación y Uso

### Requisitos
- ESP-IDF v5.0+
- Docker y Docker Compose
- Git

### Instalación Rápida
```bash
# Clonar y configurar
git clone [repositorio-fusionado]
cd tfg-nivometro-fusionado
./scripts/setup_fusion.sh

# Configurar credenciales WiFi/MQTT
nano tfg/components/communication/include/communication_secrets.h

# Compilar firmware
cd tfg && idf.py build

# Levantar stack de monitorización  
cd ../tfg_telegraf_influx_grafana && docker-compose up -d

# Flashear ESP32
cd ../tfg && idf.py flash monitor