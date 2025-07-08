# components/hcsr04p/CMakeLists.txt
idf_component_register(
    SRCS "hcsr04p.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer
)