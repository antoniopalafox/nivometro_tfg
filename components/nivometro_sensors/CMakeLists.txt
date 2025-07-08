# tfg/components/nivometro_sensors/CMakeLists.txt
idf_component_register(
    SRCS "nivometro_sensors.c"
    INCLUDE_DIRS "include"
    REQUIRES 
        hcsr04p
        hx711
        vl53l0x
        driver
        esp_timer
)