cmake_minimum_required(VERSION 3.11)

add_library(Imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)

target_include_directories(Imgui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>
    $<INSTALL_INTERFACE:include>  # <prefix>/include/mylib>
)

add_library(Imgui::Imgui ALIAS Imgui)