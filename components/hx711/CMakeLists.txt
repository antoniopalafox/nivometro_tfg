# components/hx711/CMakeLists.txt
idf_component_register(
    SRCS "hx711.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
    PRIV_REQUIRES esp_timer
)