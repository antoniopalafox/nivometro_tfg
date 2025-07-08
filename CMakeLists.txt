# Versión mínima de CMake requerida
cmake_minimum_required(VERSION 3.16)

# Incluye el script de esp-idf que configura el entorno de build
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nombre del proyecto; crea el ejecutable y agrupa los componentes
project(tfg)