cmake_minimum_required(VERSION 3.19)

project(XTVulkan)


file(GLOB SOURCE *.cpp )

add_executable(build-debug 
  src/main.cpp
  external/imgui/imgui.cpp
  external/imgui/imgui_demo.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_tables.cpp
  external/imgui/imgui_widgets.cpp
  external/imgui/backends/imgui_impl_glfw.cpp
  external/imgui/backends/imgui_impl_vulkan.cpp
)
# add_library(sample external/test/sample.cpp)
# add_library(imgui external/imgui/imgui.cpp)
target_compile_features(build-debug PRIVATE cxx_std_20)

target_include_directories( build-debug
  PRIVATE external/glm
  PRIVATE external/imgui
  PRIVATE external/imgui/backends
  PRIVATE external/stb
)

# add_library(application src/application.cpp) //Removing for now
# target_compile_features(application PRIVATE cxx_std_20)

# target_link_libraries(build-debug PRIVATE application)

target_link_libraries(build-debug PRIVATE -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

set(TEXTURE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/textures)
set(TEXTURE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/textures)

add_custom_target(create-textures
mkdir -p ${SHADER_BINARY_DIR}
  COMMAND mv -u ${TEXTURE_SOURCE_DIR} ${TEXTURE_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/textures/
)

add_custom_target(CShaders
  mkdir -p ${SHADER_BINARY_DIR}
  COMMAND glslc ${SHADER_SOURCE_DIR}/shader.vert -o ${SHADER_BINARY_DIR}/vert.spv
  COMMAND glslc ${SHADER_SOURCE_DIR}/shader.frag -o ${SHADER_BINARY_DIR}/frag.spv
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/shaders/
)
