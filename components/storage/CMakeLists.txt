idf_component_register(
    SRCS "storage.c"                      # Fichero fuente principal del módulo de storage
    INCLUDE_DIRS "include"                # Carpeta con sus archivos .h 
    REQUIRES nvs_flash nivometro_sensors            # Componentes externos necesarios para compilar y enlazar
                log
)
