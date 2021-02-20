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

    void CreateInstance();
    bool CheckValidationLayerSupport();
    std::vector<const char*> GetRequiredExtensions();
    void SetupDebugMessenger();
    QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice& device);
    bool CheckDeviceExtensionSupport(const vk::PhysicalDevice& device);
    SwapChainSupportDetails QuerySwapChainSupport(
        const vk::PhysicalDevice& device);
    bool IsDeviceSuitable(const vk::PhysicalDevice& device);
    void PickPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSurface();
    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& available_formats);
    vk::PresentModeKHR ChooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& available_present_modes);
    vk::Extent2D ChooseSwapExtent(
        const vk::SurfaceCapabilitiesKHR& capabilities);
    void CreateSwapChain();
    vk::ImageView CreateImageView(vk::Image image, vk::Format format,
                                  vk::ImageAspectFlags aspect_flags,
                                  uint32_t mip_levels);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
    void CreateFramebuffers();
    void CreateCommandPool();
    std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(
        vk::DeviceSize size, vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags properties);
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanupSwapChain();
    void RecreateSwapChain();
    void DrawFrame();
    uint32_t FindMemoryType(uint32_t type_filter,
                            vk::MemoryPropertyFlags properties);
    void CopyBuffer(vk::Buffer source, vk::Buffer destination,
                    vk::DeviceSize size);
    void CreateDescriptorSetLayout();
    void CreateUniformBuffers();
    void UpdateUniformBuffer(uint32_t index);
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    std::pair<vk::Image, vk::DeviceMemory> CreateImage(
        uint32_t width, uint32_t height, uint32_t mip_levels,
        vk::SampleCountFlagBits num_samples, vk::Format format,
        vk::ImageTiling tiling, vk::ImageUsageFlags usage,
        vk::MemoryPropertyFlags properties);
    void CreateTextureImage();
    vk::CommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(vk::CommandBuffer command_buffer);
    void TransitionImageLayout(vk::Image image, vk::Format format,
                               vk::ImageLayout old_layout,
                               vk::ImageLayout new_layout, uint32_t mip_levels);
    void CopyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width,
                           uint32_t height);
    void CreateTextureSampler();
    vk::Format FindSupportedFormat(const std::vector<vk::Format>& candidates,
                                   vk::ImageTiling tiling,
                                   vk::FormatFeatureFlags features);
    vk::Format FindDepthFormat();
    bool HasStencilComponent(vk::Format format);
    void CreateDepthResources();
    void LoadModel();
    void GenerateMipMaps(vk::Image image, vk::Format format,
                         int32_t texture_width, int32_t texture_height,
                         uint32_t mip_levels);
    vk::SampleCountFlagBits GetMaxUsableSampleCount();
    void CreateColorResources();
    void FindFontFile(std::string name);
    void CreateImGuiFramebuffers();
    void CreateImGuiCommandBuffers();
    void ResizeImGui();
    void Update(float delta_time);

    GLFWwindow* window_;
    vk::Instance instance_;
    vk::PhysicalDevice physical_device_;
    vk::Device logical_device_;
    vk::Queue graphics_queue_;
    vk::Queue present_queue_;
    vk::SurfaceKHR surface_;
    vk::SwapchainKHR swapchain_;
    uint32_t min_image_count_;
    uint32_t image_count_;
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

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    std::vector<Model> models_;

    std::vector<vk::Buffer> uniform_buffers_;
    std::vector<vk::DeviceMemory> uniform_buffers_memory_;

    vk::Image color_image_;
    vk::DeviceMemory color_image_memory_;
    vk::ImageView color_image_view_;

    uint32_t mip_levels_;
    std::optional<Texture> texture_image_;
    vk::Sampler texture_sampler_;

    vk::Image depth_image_;
    vk::DeviceMemory depth_image_memory_;
    vk::ImageView depth_image_view_;

    vk::SampleCountFlagBits msaa_samples_ = vk::SampleCountFlagBits::e1;

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