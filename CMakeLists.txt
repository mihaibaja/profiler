cmake_minimum_required(VERSION 3.20)
project(profiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# SDL3 y SDL3_ttf viven en /home/mihai/Documents/SDL3 (prefijo con
# include/, lib/ y lib/cmake/). Si se pasan por -DCMAKE_PREFIX_PATH o si
# estan instalados en el sistema, esto no estorba.
if(NOT DEFINED SDL3_DIR AND EXISTS "/home/mihai/Documents/SDL3/lib/cmake/SDL3")
    list(APPEND CMAKE_PREFIX_PATH "/home/mihai/Documents/SDL3")
endif()

find_package(SDL3 CONFIG REQUIRED)
find_package(SDL3_ttf CONFIG REQUIRED)

add_executable(profiler src/main.cpp src/pie_chart.cpp)

target_compile_options(profiler PRIVATE -Wall -Wextra)

# Los logs de depuracion van tras #ifdef DEBUG: solo existen en configuracion
# Debug (cmake -DCMAKE_BUILD_TYPE=Debug).
target_compile_definitions(profiler PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# Ruta de la fuente, absoluta y resuelta al configurar: el binario corre desde
# build/, asi que una ruta relativa no valdria.
set(PROFILER_FONT_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/Montserrat/static/Montserrat-Regular.ttf")
target_compile_definitions(profiler PRIVATE
    PROFILER_FONT_PATH="${PROFILER_FONT_PATH}")
target_link_libraries(profiler PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)
