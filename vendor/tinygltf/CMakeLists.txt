cmake_minimum_required(VERSION 3.20)

project(tinygltf VERSION 1.0.0 LANGUAGES CXX)

add_library(${PROJECT_NAME}
	"tiny_gltf.cc"
)
target_include_directories(${PROJECT_NAME} PUBLIC
	"include"
)
