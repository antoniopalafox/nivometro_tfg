idf_component_register(
    SRCS "config.c"                             # Fichero fuente principal del módulo de config
    INCLUDE_DIRS "include"                      # Carpeta con sus archivos .h
    REQUIRES nvs_flash                          # Componentes externos necesarios para compilar y enlazar
)
