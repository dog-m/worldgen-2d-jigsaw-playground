cmake_minimum_required(VERSION 3.15)

project(worldgen_2d_playground)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(${PROJECT_NAME}
    src/tiles.cpp
    src/world.cpp
    src/structures.cpp
    src/worldgen.cpp
    src/main.cpp
    src/thirdparty/noise/noise1234.c
)

target_link_libraries(${PROJECT_NAME}
    ${CONAN_LIBS}
)
