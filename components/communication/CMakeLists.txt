idf_component_register(
    SRCS        "communication.c"              # Fichero fuente principal del módulo de comunicación
    INCLUDE_DIRS "include"                     # Carpeta con sus archivos .h
    REQUIRES                                   # Componentes externos necesarios para compilar y enlazar
    esp_netif esp_wifi 
    esp_event 
    mqtt 
    freertos 
    nivometro_sensors
)

