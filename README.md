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

#### Configurar credenciales WiFi/MQTT
idf.py menuconfig

Serial flasher config -> Flash Size (4 MB)

Component config -> Communication configuration. Rellena SSID, Pasword y MQTT URI

#### Compilar
idf.py build 

#### Levantar stack de monitorización  
cd tfg_telegraf_influx_grafana && docker-compose up -d

#### Flashear ESP32
cd .. && idf.py flash monitor

===============================================================================



🔄 Para usar con hardware real:
Cuando tengas el circuito de detección USB/Batería físico, solo necesitas:

Cambiar una línea en power_manager.c:
c// CAMBIAR DE:
static bool simulation_enabled = true;

// A:
static bool simulation_enabled = false;

El resto funciona automáticamente con el GPIO real

📊 Resumen de rendimiento:
ModoIntervalo mediciónComportamientoDuración batería estimadaUSB5 segundosActivo continuo♾️ InfinitaBatería60 segundos + sleepDeep sleep automático🔋 15-30 días
🏆 ¡ENHORABUENA!
Has implementado exitosamente un sistema de gestión inteligente de energía para tu nivómetro que:

⚡ Maximiza el rendimiento cuando hay alimentación externa
🔋 Maximiza la duración de batería cuando funciona autónomamente
🔄 Se adapta automáticamente sin intervención manual
💤 Gestiona deep sleep de forma inteligente
📡 Mantiene conectividad cuando es necesario

¡Tu nivómetro está listo para funcionar de forma completamente autónoma en la Antártida! 🏔️❄️