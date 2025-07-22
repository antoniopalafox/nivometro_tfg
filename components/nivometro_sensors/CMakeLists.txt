# tfg/components/nivometro_sensors/CMakeLists.txt
idf_component_register(
    SRCS 
        "src/nivometro_sensors.c"
        "src/hcsr04p.c"
        "src/hx711.c"
        "src/vl53l0x.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        #hcsr04p
        #hx711
        #vl53l0x
        driver
        esp_timer
)