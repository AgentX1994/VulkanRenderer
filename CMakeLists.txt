cmake_minimum_required(VERSION 3.11.0)
project(VulkanRenderer)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Shaderc REQUIRED IMPORTED_TARGET shaderc)

add_executable(VulkanTest
    src/main.cpp
    src/application.cpp
    src/utils.cpp
    src/stb_image.cpp
    src/tiny_obj_loader.cpp)
target_include_directories(VulkanTest
PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)

target_link_libraries(VulkanTest
PUBLIC
    Vulkan::Vulkan
    glfw
    PkgConfig::Shaderc
    glm
    ${CMAKE_DL_LIBS})

add_custom_target(
  copy_shader_files
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_CURRENT_BINARY_DIR}/shaders
  COMMENT "Copying shaders" VERBATIM
)

add_custom_target(
  copy_texture_files
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/textures ${CMAKE_CURRENT_BINARY_DIR}/textures
  COMMENT "Copying textures" VERBATIM
)

add_custom_target(
  copy_model_files
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models ${CMAKE_CURRENT_BINARY_DIR}/models
  COMMENT "Copying models" VERBATIM
)

add_dependencies(VulkanTest copy_shader_files)
add_dependencies(VulkanTest copy_texture_files)
add_dependencies(VulkanTest copy_model_files)