# Nivómetro para monitor de neutrones Estación Antártica Juan Carlos I


Trabajo Fin de Grado en Ingeniería Telemática · Universidad de Alcalá
**Profesor**: Óscar García Población
**Alumnos**: Antonio Mata Marco y Antonio Palafox Moya 


##  Tabla de Contenidos

- [Descripción](#descripción)
- [Tecnologías](#tecnologías)
- [Instalación y Uso](#instalación-y-uso)
- [Menuconfig](#menuconfig)
- [Estados del LED](#estados-del-led)
- [Modo Calibración](#modo-calibración)
- [Variables de entorno .env](#variables-de-entorno-env)
- [Configurar InfluxDB](#configurar-influxdb)
- [Configurar Grafana](#configurar-grafana)


## Descripción

Se ha desarrollado una **estación multisensor IoT autónoma** para medir y transmitir en tiempo real la acumulación de nieve en la estación Antártica. El sistema combina:

- **HC-SR04P (sensor de ultrasonidos)**  
  Mide el espesor de la capa de nieve, diseñado para entornos extremos y resistente al agua.  
- **Celda de carga + HX711 (sensor de peso)**  
  Estima el peso acumulado, permitiendo corregir posibles sesgos de lectura de neutrones por la compactación de la nieve.  

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

2. **Configuración Menuconfig**  
   Para ejecutar este paso puedes recibir mas informacion aqui: [Menuconfig](#menuconfig)
   
   ```bash
   idf.py menuconfig

3. **Compilación**

    ```bash
    idf.py build

4. **Configurar variables de entorno .env**  
   Si necesitas informacion sobre este punto pulsa aquí: [Variables de entorno .env](#variables-de-entorno-env)

    ```bash
    cd tfg_telegraf_influx_grafana
    cp .env.example .env


5. **Desplegar monitorización**  
    ```bash
    cd tfg_telegraf_influx_grafana
    docker-compose up -d
    xdg-open "http://localhost:3000" #Grafana
    xdg-open "http://localhost:8086" #InfluxDB
    ```
   Clicando estos enlaces puedes configurar tus menus de [InfluxDB](#configurar-influxdb) y [Grafana](#configurar-grafana).

6. **Ejecutar ESP32**
   ```bash
   cd ..
   idf.py flash monitor.
   ```
   A partir de aquí siga las instruciones que le indican por la linea de comandos
   Para salir de la consola Crtl+T y seguido Ctrl+X.
   
## Menuconfig

Q: Guardar y salir   Esc: Retroceder   Enter: Confirmar  ?: Para obtener informacion del prompt

   Serial Flasher Config → Flash Size: 4 MB
   
   Calibración del Nivómetro:
   Configuración de los parametros de calibración de ambos senrores y del proceso de calibración.

   Comunicación Nivómetro:
   Rellenar WiFi SSID, WiFi Password y MQTT Broker URI con la configuración deseada.

## Estados del LED

Es proyecto tiene un programado un LED rojo externo el cual va a indiar en que estado se encuentra el sistema.
🟢 NORMAL: Parpadeo cada 2 segundos (muy lento)
🟡 WARNING: Parpadeo cada 0.8 segundos (medio)
🔵 CALIBRATION: Parpadeo cada 0.2 segundos (rápido)
🔴 ERROR: Parpadeo cada 0.075 segundos (muy rápido)
⚪ PROCESO COMPLETO: encendido fijo

## Modo Calibración

Para entrar en modo calibración debes mantén presionado **BOOT** durante los segundos que se configuren en el [menuconfig](#menuconfig) al arranque y seguir las instrucciones detalladas en el propio menuconfig pulsando la tecla ?.

## Variables de entorno .env

Abre .env en tu editor y completa con tus datos:

   DOCKER_INFLUXDB_INIT_USERNAME=<tu_usuario_influx>
   DOCKER_INFLUXDB_INIT_PASSWORD=<tu_password_influx>
   DOCKER_INFLUXDB_INIT_ADMIN_TOKEN=<tu_token_influx>
   GF_ADMIN_USER=<tu_usuario_grafana>
   GF_ADMIN_PASSWORD=<tu_password_grafana>

## Configurar InfluxDB

## Configurar Grafana