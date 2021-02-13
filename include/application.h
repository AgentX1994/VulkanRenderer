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
    vk::ImageView CreateImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspect_flags);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
    void CreateFramebuffers();
    void CreateCommandPool();
    std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void DrawFrame();
    uint32_t FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties);
    void CopyBuffer(vk::Buffer source, vk::Buffer destination, vk::DeviceSize size);
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void UpdateUniformBuffer(uint32_t index);
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    std::pair<vk::Image, vk::DeviceMemory> CreateImage(
        uint32_t width,
        uint32_t height,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties);
    void CreateTextureImage();
    vk::CommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(vk::CommandBuffer command_buffer);
    void TransitionImageLayout(
        vk::Image image,
        vk::Format format,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout);
    void CopyBufferToImage(
        vk::Buffer buffer,
        vk::Image image,
        uint32_t width,
        uint32_t height);
    void CreateTextureImageView();
    void CreateTextureSampler();
    vk::Format FindSupportedFormat(
        const std::vector<vk::Format>& candidates,
        vk::ImageTiling tiling,
        vk::FormatFeatureFlags features);
    vk::Format FindDepthFormat();
    bool HasStencilComponent(vk::Format format);
    void CreateDepthResources();

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
    vk::DescriptorSetLayout descriptor_set_layout_;
    vk::PipelineLayout pipeline_layout_;
    std::vector<vk::Framebuffer> swap_chain_frame_buffers_;
    vk::CommandPool command_pool_;
    vk::DescriptorPool descriptor_pool_;
    std::vector<vk::DescriptorSet> descriptor_sets_;

    vk::Buffer vertex_buffer_;
    vk::DeviceMemory vertex_buffer_memory_;
    vk::Buffer index_buffer_;
    vk::DeviceMemory index_buffer_memory_;
    std::vector<vk::Buffer> uniform_buffers_;
    std::vector<vk::DeviceMemory> uniform_buffers_memory_;

    vk::Image texture_image_;
    vk::DeviceMemory texture_image_memory_;
    vk::ImageView texture_image_view_;
    vk::Sampler texture_sampler_;

    vk::Image depth_image_;
    vk::DeviceMemory depth_image_memory_;
    vk::ImageView depth_image_view_;

    std::vector<vk::CommandBuffer> command_buffers_;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> image_available_semaphore_;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> render_finished_semaphore_;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> in_flight_fences_;
    std::vector<vk::Fence> images_in_flight_;
    size_t current_frame_ = 0;
    bool framebuffer_resized_ = false;
    vk::DebugUtilsMessengerEXT debug_messenger_;
};