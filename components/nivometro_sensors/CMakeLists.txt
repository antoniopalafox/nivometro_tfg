# tfg/components/nivometro_sensors/CMakeLists.txt
idf_component_register(
    SRCS 
        "src/nivometro_sensors.c"
        "src/hcsr04p.c"
        "src/hx711.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        driver
        esp_timer
)