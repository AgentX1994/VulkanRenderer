#pragma once

#include <array>
#include <optional>

#include "common.h"
#include "common_vulkan.h"

#include "imgui.h"
#include "vertex.h"
#include "model.h"
#include "texture.h"
#include "scene_graph.h"
#include "render_object.h"
#include "camera.h"

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct FrameData {
    std::optional<GpuBuffer> camera_uniform_buffer;
    vk::DescriptorSet camera_uniform_descriptor;
    vk::Semaphore image_available_semaphore;
    vk::Semaphore render_finished_semaphore;
    vk::Fence in_flight_fence;
};

class Application
{
public:
    void Run();

    void SetFramebufferResized();
    void SetRenderScaling(float scale);

private:
    void InitWindow();
    void InitVulkan();
    void SetupImgui();
    void MainLoop();
    void Cleanup();

    void CreateRenderer();
    void SetupDebugMessenger();
    
    std::vector<const char*> GetRequiredExtensions();
    void CreateCommandBuffers();
    void CreateFrameData();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void DrawFrame();
    void UpdateCameraBuffer();
    void CreateCameraDescriptorSets();
    void LoadScene();
    void DrawScene(FrameData& frame_data, vk::Framebuffer& framebuffer, vk::CommandBuffer& command_buffer);

    void FindFontFile(std::string name);
    void CreateImGuiFramebuffers();
    void CreateImGuiCommandBuffers();
    void ResizeImGui();
    void Update(float delta_time);

    GLFWwindow* window_;
    std::optional<RendererState> renderer_;

    SceneGraph scene_graph_;
    std::vector<Model> models_;
    std::vector<RenderObject> render_objects_;
    Camera camera_;

    std::vector<GpuBuffer> uniform_buffers_;

    std::vector<vk::CommandBuffer> command_buffers_;

    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frame_data_;
    std::vector<vk::Fence> images_in_flight_;

    bool imgui_display_ = false;
    vk::DescriptorPool imgui_descriptor_pool_;
    vk::RenderPass imgui_render_pass_;
    vk::CommandPool imgui_command_pool_;
    std::vector<vk::CommandBuffer> imgui_command_buffers_;
    std::vector<vk::Framebuffer> imgui_frame_buffers_;

    std::string font_file_;
    ImGuiStyle imgui_style_;

    size_t current_frame_ = 0;
    float frames_per_second_ = 0.0f;
    bool framebuffer_resized_ = false;
    float window_scaling_ = 1.0f;
    float rotation_rate_ = 1.0f;
    float current_model_rotation_degrees_ = 0.0f;
    vk::DebugUtilsMessengerEXT debug_messenger_;
};