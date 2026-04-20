cmake_minimum_required(VERSION 3.20)
project(imgui VERSION 1.0.0 LANGUAGES CXX)

file(GLOB SOURCES "src/*.cpp")
add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC "include")
target_link_libraries(${PROJECT_NAME}
    glfw
)
