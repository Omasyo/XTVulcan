cmake_minimum_required(VERSION 3.19)

project(XTVulkan)

add_executable(build-debug src/main.cpp)
add_library(application src/application.cpp)

target_link_libraries(build-debug PRIVATE application)

# target_compile_options(build-debug PRIVATE -std=c++20)
target_compile_features(build-debug PRIVATE cxx_std_20)
target_compile_features(application PRIVATE cxx_std_20)
target_link_libraries(build-debug PRIVATE -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

file(GLOB SHADERS
  ${SHADER_SOURCE_DIR}/*.vert
  ${SHADER_SOURCE_DIR}/*.frag
  ${SHADER_SOURCE_DIR}/*.comp
  ${SHADER_SOURCE_DIR}/*.geom
  ${SHADER_SOURCE_DIR}/*.tesc
  ${SHADER_SOURCE_DIR}/*.tese
  ${SHADER_SOURCE_DIR}/*.mesh
  ${SHADER_SOURCE_DIR}/*.task
  ${SHADER_SOURCE_DIR}/*.rgen
  ${SHADER_SOURCE_DIR}/*.rchit
  ${SHADER_SOURCE_DIR}/*.rmiss)