cmake_minimum_required(VERSION 3.17.0)
project(VulkanRenderer)

set(CMAKE_CXX_STANDARD 17)

option (FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." OFF)
if (${FORCE_COLORED_OUTPUT})
  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options (-fdiagnostics-color=always)
  elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options (-fcolor-diagnostics)
  endif ()
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Dependency management on Windows sucks
# Just create some targets for now
find_package(Vulkan REQUIRED)
if (WIN32)
  add_library(glfw SHARED IMPORTED)
  set_property(TARGET glfw PROPERTY IMPORTED_LOCATION "C:\\local\\glfw-3.3.3.bin.WIN64\\lib-vc2019\\glfw3.dll")
  set_property(TARGET glfw PROPERTY IMPORTED_IMPLIB "C:\\local\\glfw-3.3.3.bin.WIN64\\lib-vc2019\\glfw3dll.lib")  
  set_property(TARGET glfw PROPERTY INTERFACE_INCLUDE_DIRECTORIES "C:\\local\\glfw-3.3.3.bin.WIN64\\include")
  
  add_library(glm INTERFACE IMPORTED)
  set_property(TARGET glm PROPERTY INTERFACE_INCLUDE_DIRECTORIES "C:\\local\\glm")
  
  
  add_library(shaderc STATIC IMPORTED)
  set_property(TARGET shaderc PROPERTY IMPORTED_LOCATION "C:\\local\\shaderc\\lib\\shaderc_combined.lib")
  set_property(TARGET shaderc PROPERTY INTERFACE_INCLUDE_DIRECTORIES "C:\\local\\shaderc\\include")
elseif(UNIX)
  find_package(glfw3 REQUIRED)
  find_package(glm REQUIRED)
  find_package(Fontconfig REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(Shaderc REQUIRED IMPORTED_TARGET shaderc)
endif()

add_subdirectory(third_party)

add_library(VulkanTestLib STATIC
  src/application.cpp
  src/input.cpp
  src/utils.cpp
  src/vertex.cpp
  src/mesh.cpp
  src/model.cpp
  src/scene_graph.cpp
  src/scene_node.cpp
  src/render_object.cpp
  src/camera.cpp
  src/material.cpp
  src/texture.cpp
  src/material_cache.cpp
  src/texture_cache.cpp
  src/swapchain.cpp
  src/renderer_state.cpp
  src/gpu_buffer.cpp
  src/gpu_image.cpp
  src/stb_image.cpp
  src/tiny_obj_loader.cpp)
  
target_include_directories(VulkanTestLib
PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib
)

if (WIN32)
  target_link_libraries(VulkanTestLib
  PUBLIC
    Imgui::Imgui
    Vulkan::Vulkan
    glfw
    shaderc
    ${CMAKE_DL_LIBS})

  set_property(TARGET VulkanTestLib PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded")
elseif (UNIX)
  target_link_libraries(VulkanTestLib
  PUBLIC
    Imgui::Imgui
    Vulkan::Vulkan
    glfw
    Fontconfig::Fontconfig
    PkgConfig::Shaderc
    ${CMAKE_DL_LIBS})
endif()

add_executable(VulkanTest
  src/main.cpp
)

if (WIN32)
  set_property(TARGET VulkanTest PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

target_link_libraries(VulkanTest
PUBLIC
  VulkanTestLib)

# On my Ubuntu install, glm is glm::glm,
# But on Arch, it is glm
# link with whichever exists
if (TARGET glm::glm)
  target_link_libraries(VulkanTestLib
  PUBLIC
    glm::glm
  )
else()
  target_link_libraries(VulkanTestLib
  PUBLIC
    glm
  )
endif()

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

add_custom_target(
  copy_material_files
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/materials ${CMAKE_CURRENT_BINARY_DIR}/materials
  COMMENT "Copying materials" VERBATIM
)

add_dependencies(VulkanTest copy_shader_files)
add_dependencies(VulkanTest copy_texture_files)
add_dependencies(VulkanTest copy_model_files)
add_dependencies(VulkanTest copy_material_files)

# Don't feel list supporting testing on Windows right now
if (UNIX)
  enable_testing()
  add_subdirectory(tests)
endif()