cmake_minimum_required(VERSION 3.27)

project(ScomTreeFor2D)

add_executable(ScomTreeFor2D ./src/main.cpp)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(./glfw)
target_link_libraries(ScomTreeFor2D glfw)

add_subdirectory(./glad)
target_link_libraries(ScomTreeFor2D Glad)
