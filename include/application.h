#pragma once

#include <array>
#include <fstream>
#include <optional>

#include "camera.h"
#include "common.h"
#include "common_vulkan.h"
#include "imgui.h"
#include "model.h"
#include "render_object.h"
#include "scene_graph.h"
#include "texture.h"
#include "vertex.h"
#include "input.h"

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct FrameData
{
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

private:
    // Main functions
    void Init();
    void MainLoop();
    void Update(double delta_time);
    void Render();

    // Cleanup functions
    void CleanupSwapChain();
    void Cleanup();

    // GLFW functions
    void InitWindow();

    // GLFW Callbacks
    friend struct GlfwCallbacks;
    void SetFramebufferResized();
    void SetRenderScaling(float scale);

    // Vulkan Setup functions
    void InitVulkan();
    void CreateRenderer();
    void SetupDebugMessenger();
    std::vector<const char*> GetRequiredExtensions();
    void CreateCommandBuffers();
    void CreateFrameData();
    void RecreateSwapChain();
    void CreateCameraDescriptorSets();

    // Scene Setup Function
    void LoadScene();

    // Update functions
    void UpdateRotatingCamera(double delta_time);
    void UpdateControlledCamera(double delta_time);

    // Rendering functions
    void WaitForNextFrameFence();
    std::optional<uint32_t> GetNextImage();
    void WaitForImageFenceAndSetNewFence(uint32_t image_index);
    void UpdateCameraUniformBuffer();
    void DrawScene(FrameData& frame_data, vk::Framebuffer& framebuffer,
                   vk::CommandBuffer& command_buffer);
    void DrawGui(vk::Framebuffer& framebuffer,
                 vk::CommandBuffer& command_buffer,
                 vk::SampleCountFlagBits& msaa_samples);
    void SubmitGraphicsCommands(std::vector<vk::CommandBuffer> command_buffers);
    void Present(uint32_t image_index);

    // Imgui Functions
    void SetupImgui();
    void FindFontFile(std::string name);
    void CreateImGuiFramebuffers();
    void CreateImGuiCommandBuffers();
    void ResizeImGui();

    // GLFW Window
    GLFWwindow* window_;

    // Main Vulkan Context
    std::optional<RendererState> renderer_;

    // Scene details
    SceneGraph scene_graph_;
    std::vector<Model> models_;
    std::vector<RenderObject> render_objects_;

    // Camera details
    std::array<Camera,2> cameras_;
    NonOwningPointer<Camera> rotating_camera_; //< cameras_[0], rotates around scene
    NonOwningPointer<Camera> controlled_camera_; //< cameras_[1], stationary
    NonOwningPointer<Camera> active_camera_; //< Whichever camera is active

    // Command buffers (one per swapchain image)
    std::vector<vk::CommandBuffer> command_buffers_;

    // Per frame uniform and sync data
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frame_data_;

    // Per swapchain fences (to avoid concurrent writes to same swapchain image)
    std::vector<vk::Fence> images_in_flight_;

    // input handling
    std::optional<Input> input_;

    // Imgui Data
    bool imgui_display_ = false;
    bool imgui_toggle_pressed_last_frame_ = false;
    vk::DescriptorPool imgui_descriptor_pool_;
    vk::RenderPass imgui_render_pass_;
    vk::CommandPool imgui_command_pool_;
    std::vector<vk::CommandBuffer> imgui_command_buffers_;
    std::vector<vk::Framebuffer> imgui_frame_buffers_;
    std::string font_file_;
    ImGuiStyle imgui_style_;

    // statistics data
    size_t current_frame_ = 0;
    float current_frames_per_second_ = 0.0f;
    double fps_timer_ = 0.0;
    std::vector<float> frames_per_second_data_;

    // Whether the framebuffer has been resized and a swapchain recreation is
    // needed
    bool framebuffer_resized_ = false;

    // Current window DPI scaling
    float window_scaling_ = 1.0f;

    // Rotation rate of the camera
    float rotation_rate_ = 1.0f;
    // Current rotation of the camera
    float current_camera_rotation_degrees_ = 0.0f;
    // Movement speed of controlled camera
    float camera_movement_speed_ = 3.0f;
    // Roll speed of controlled camera
    float camera_roll_speed_ = 30.0f;
    // How much to slow movements when slow key is held
    float slowdown_factor_ = 0.25;

    // Vulkan debug messenger (unused)
    vk::DebugUtilsMessengerEXT debug_messenger_;
    // Validation error log file
    // Only valid if validation layers are enabled
    std::ofstream log_file_;
};