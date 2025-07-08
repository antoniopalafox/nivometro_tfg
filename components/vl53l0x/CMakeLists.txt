# components/vl53l0x/CMakeLists.txt
idf_component_register(
    SRCS "vl53l0x.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)