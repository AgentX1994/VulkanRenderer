#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <shaderc/shaderc.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;

    inline bool IsComplete()
    {
        return graphics_family.has_value() && present_family.has_value();
    }
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

class Application {
public:
    void Run();

    void SetFramebufferResized();

private:
    void InitWindow();
    void InitVulkan();
    void MainLoop();
    void Cleanup();

    void CreateInstance();
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
    void SetupDebugMessenger();
    QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice& device);
    bool CheckDeviceExtensionSupport(const vk::PhysicalDevice& device);
    SwapChainSupportDetails QuerySwapChainSupport(const vk::PhysicalDevice& device);
    bool IsDeviceSuitable(const vk::PhysicalDevice& device);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes);
    vk::Extent2D ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
    void CreateFramebuffers();
    void CreateCommandPool();
    void CreateSyncObjects();
    void CreateCommandBuffers();
    void CleanupSwapChain();
    void RecreateSwapChain();

    void DrawFrame();

    GLFWwindow* window_;
    vk::Instance instance_;
    vk::PhysicalDevice physical_device_;
    vk::Device logical_device_;
    vk::Queue graphics_queue_;
    vk::Queue present_queue_;
    vk::SurfaceKHR surface_;
    vk::SwapchainKHR swapchain_;
    std::vector<vk::Image> swap_chain_images_;
    vk::Format swap_chain_image_format_;
    vk::Extent2D swap_chain_extent_;
    std::vector<vk::ImageView> swap_chain_image_views_;
    vk::RenderPass render_pass_;
    vk::Pipeline graphics_pipeline_;
    vk::PipelineLayout pipeline_layout_;
    std::vector<vk::Framebuffer> swap_chain_frame_buffers_;
    vk::CommandPool command_pool_;
    std::vector<vk::CommandBuffer> command_buffers_;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> image_available_semaphore_;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> render_finished_semaphore_;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> in_flight_fences_;
    std::vector<vk::Fence> images_in_flight_;
    size_t current_frame_ = 0;
    bool framebuffer_resized_ = false;
    vk::DebugUtilsMessengerEXT debug_messenger_;
};