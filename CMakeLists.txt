cmake_minimum_required(VERSION 3.19)

project(XTVulkan)

include_directories(
  external/glm
  external/imgui
  src
)

add_executable(build-debug src/main.cpp)
target_compile_features(build-debug PRIVATE cxx_std_20)

# add_library(application src/application.cpp) //Removing for now
# target_compile_features(application PRIVATE cxx_std_20)

# target_link_libraries(build-debug PRIVATE application)

target_link_libraries(build-debug PRIVATE -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

add_custom_target(CShaders
  mkdir -p ${SHADER_BINARY_DIR}
  COMMAND glslc ${SHADER_SOURCE_DIR}/shader.vert -o ${SHADER_BINARY_DIR}/vert.spv
  COMMAND glslc ${SHADER_SOURCE_DIR}/shader.frag -o ${SHADER_BINARY_DIR}/frag.spv
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/
)
