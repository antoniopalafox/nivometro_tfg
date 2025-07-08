idf_component_register(
    SRCS "utils.c"                 # Fichero fuente principal del módulo de utils
    INCLUDE_DIRS "include"         # Carpeta con sus archivos .h 
    REQUIRES nivometro_sensors log           # Componentes externos necesarios para compilar y enlazar
)

