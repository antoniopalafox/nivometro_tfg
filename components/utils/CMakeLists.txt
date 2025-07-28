#File: components/utils/CMakeLists.txt
idf_component_register(
    SRCS "utils.c"                 # Fichero fuente principal del módulo de utils
    INCLUDE_DIRS "include"         # Carpeta con sus archivos .h 
    REQUIRES 
        nivometro_sensors          # Para las estructuras de datos de sensores
        log                        # Para ESP_LOG funciones
        nvs_flash                  # Para gestión NVS de calibración
        driver                     # Para GPIO (LED y botón BOOT)
        esp_timer                  # Para timestamps y delays
        freertos                   # Para tareas FreeRTOS del LED
)