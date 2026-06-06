cmake_minimum_required(VERSION 3.31)
project(funrun)
set(CMAKE_CXX_STANDARD 20)

add_executable(funrun
    src/main.cpp
    src/Game.cpp
    src/EntityFactory.cpp
    src/Systems.cpp
    lib/bagel26/bagel.h)

target_include_directories(funrun PRIVATE src lib/bagel26)

set(SDL_STATIC ON)
add_subdirectory(lib/SDL)
target_link_libraries(funrun PUBLIC SDL3-static)

add_subdirectory(lib/SDL_image)
target_link_libraries(funrun PUBLIC SDL3_image-static)

set(BOX2D_SAMPLES OFF)
set(BOX2D_BENCHMARKS OFF)
set(BOX2D_DOCS OFF)
set(BOX2D_UNIT_TESTS OFF)
add_subdirectory(lib/box2d)
target_link_libraries(funrun PUBLIC box2d)

# Copy resources
add_custom_command(TARGET funrun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    "${PROJECT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:funrun>/res")
