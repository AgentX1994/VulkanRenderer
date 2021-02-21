#pragma once

#include <array>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <shaderc/shaderc.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "imgui.h"
#include "vertex.h"
#include "model.h"
#include "texture.h"

struct QueueFamilyInfo
{
    uint32_t index;
    vk::QueueFamilyProperties properties;
};

struct QueueFamilyIndices
{
    std::optional<QueueFamilyInfo> graphics_family;
    std::optional<QueueFamilyInfo> present_family;

    inline bool IsComplete()
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

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
    void CreateSyncObjects();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void DrawFrame();
    void CreateUniformBuffers();
    void UpdateUniformBuffer(uint32_t index);
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateTextureImage();
    void CreateTextureSampler();
    void LoadModel();

    void FindFontFile(std::string name);
    void CreateImGuiFramebuffers();
    void CreateImGuiCommandBuffers();
    void ResizeImGui();
    void Update(float delta_time);

    GLFWwindow* window_;
    std::optional<RendererState> renderer_;
    vk::DescriptorPool descriptor_pool_;
    std::vector<vk::DescriptorSet> descriptor_sets_;

    std::vector<Model> models_;

    std::vector<GpuBuffer> uniform_buffers_;

    std::optional<Texture> texture_image_;
    vk::Sampler texture_sampler_;

    std::vector<vk::CommandBuffer> command_buffers_;

    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> image_available_semaphore_;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> render_finished_semaphore_;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> in_flight_fences_;
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