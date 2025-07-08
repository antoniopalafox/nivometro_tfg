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
        # Drivers específicos nivómetro (antoniopalafox)
        hcsr04p
        hx711
        vl53l0x
        # Componente integración
        nivometro_sensors
        # Dependencias del sistema
        nvs_flash
        driver
)