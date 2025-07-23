idf_component_register(
    SRCS         "power_manager.c"   # tu fichero fuente
    INCLUDE_DIRS "include"           # tu carpeta de headers
    REQUIRES 
        driver 
        esp_pm 
        log  
        esp_timer
)