idf_component_register(
    SRCS        "diagnostics.c"                    # Fichero fuente principal del módulo de diagnostics 
    INCLUDE_DIRS "include"                         # Carpeta con sus archivos .h 
    REQUIRES    nvs_flash                          # Componentes externos necesarios para compilar y enlazar
                log
)
