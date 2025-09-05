idf_component_register(
    SRCS    "tasks.c"                  # Fichero fuente principal del módulo de tasks
    INCLUDE_DIRS "include"             # Carpeta con sus archivos .h 
    REQUIRES    freertos               # Componentes externos necesarios para compilar y enlazar
                utils      
                storage
                communication
                power_manager
                nivometro_sensors
)