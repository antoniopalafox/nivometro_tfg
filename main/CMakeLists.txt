# tfg/main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        # Componentes base (Antonio Mata)
        communication
        power_manager
        config
        diagnostics  
        storage
        tasks
        utils
        # Componente integración
        nivometro_sensors
        # Dependencias del sistema
        nvs_flash
        driver
)