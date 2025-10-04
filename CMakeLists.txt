cmake_minimum_required(VERSION 3.27)

set(nameProject "ScomTreeFor2D")
project(nameProject)

add_executable(${nameProject}
    ./src/graphicalInterface.cpp
    ./src/clipmap.cpp
    ./src/global.cpp
    ./src/cameraControl.cpp
    ./src/shaders.cpp
)

file(COPY ./shaders DESTINATION ${CMAKE_BINARY_DIR})

target_include_directories(${nameProject} PRIVATE 
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(./glfw)
target_link_libraries(${nameProject} glfw)

add_subdirectory(./glad)
target_link_libraries(${nameProject} Glad)

target_compile_features(${nameProject} PRIVATE cxx_std_17)