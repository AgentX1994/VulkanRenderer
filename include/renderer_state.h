#pragma once

#include <optional>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

// TODO Find a way to make this work with other window libraries?
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "swapchain.h"
#include "texture_cache.h"
#include "material_cache.h"

class Swapchain;

class RendererState
{
public:
    RendererState(const std::string name, GLFWwindow* window,
                  const std::vector<const char*>& required_instance_extensions,
                  const std::vector<const char*>& required_device_extensions,
                  const std::vector<const char*>& layers);

    RendererState(const RendererState&) = delete;
    RendererState(RendererState&&) = delete;

    RendererState& operator=(const RendererState&) = delete;
    RendererState& operator=(RendererState&&) = delete;

    ~RendererState();

    vk::Instance& GetInstance();
    vk::SurfaceKHR& GetSurface();

    vk::PhysicalDevice& GetPhysicalDevice();
    vk::Device& GetDevice();

    vk::CommandPool& GetGraphicsCommandPool();
    vk::Queue& GetGraphicsQueue();
    vk::Queue& GetPresentQueue();

    vk::CommandPool& GetTransientCommandPool();
    vk::Queue& GetTransferQueue();

    TextureCache& GetTextureCache();
    MaterialCache& GetMaterialCache();

    vk::SampleCountFlagBits GetMaxSampleCount();

    void RecreateSwapchain(GLFWwindow* window);
    Swapchain& GetSwapchain();

    vk::RenderPass& GetRenderPass();

    vk::DescriptorSetLayout& GetDescriptorSetLayout();

    vk::ImageView& GetColorImageView();
    vk::ImageView& GetDepthImageView();

    vk::CommandBuffer BeginSingleTimeCommands();
    void EndSingleTimeCommands(vk::CommandBuffer command_buffer);

    struct QueueFamilyInfo
    {
        uint32_t index;
        vk::QueueFamilyProperties properties;
    };

    struct QueueFamilyIndices
    {
        std::optional<QueueFamilyInfo> graphics_family;
        std::optional<QueueFamilyInfo> present_family;
        std::optional<QueueFamilyInfo> transfer_family;

        inline bool IsComplete()
        {
            return graphics_family.has_value() && present_family.has_value() &&
                   transfer_family.has_value();
        }
    };

    struct SwapChainSupportDetails
    {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> present_modes;
    };

    SwapChainSupportDetails QuerySwapChainSupport(
        const vk::PhysicalDevice& device);

    QueueFamilyIndices GetQueueFamilies();

private:
    bool CheckValidationLayerSupport(const std::vector<const char*>& layers);

    bool CheckInstanceExtensions(
        const std::vector<vk::ExtensionProperties> supported_extensions,
        std::vector<const char*> required_extensions) const;

    vk::Instance CreateInstance(
        std::string name, const std::vector<const char*>& required_extensions,
        const std::vector<const char*>& layers);

    vk::SurfaceKHR CreateSurface(GLFWwindow* window);

    vk::QueueFamilyProperties GetQueueFamilyPropertiesByIndex(uint32_t index);

    QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice& device);

    bool CheckDeviceExtensionSupport(
        const std::vector<const char*> required_extensions,
        const vk::PhysicalDevice& device);

    bool IsDeviceSuitable(const std::vector<const char*> required_extensions,
                          const vk::PhysicalDevice& device);

    vk::SampleCountFlagBits GetMaxUsableSampleCount();

    vk::PhysicalDevice CreatePhysicalDevice(
        const std::vector<const char*> required_extensions);

    std::tuple<vk::Device, vk::Queue, vk::Queue, vk::Queue>
    CreateDeviceAndQueues(const std::vector<const char*> extensions,
                          const std::vector<const char*> layers);
    
    void CreateColorResources();
    void CreateDepthResources();

    vk::CommandPool CreateCommandPool(uint32_t queue_index);

    vk::RenderPass CreateRenderPass();

    vk::DescriptorSetLayout CreateDescriptorSetLayout();

    vk::Instance instance_;

    vk::SurfaceKHR surface_;

    vk::PhysicalDevice physical_device_;

    QueueFamilyIndices queue_families_;

    vk::Device device_;

    std::optional<Swapchain> swapchain_;

    std::optional<GpuImage> color_image_;
    vk::ImageView color_image_view_;

    std::optional<GpuImage> depth_image_;
    vk::ImageView depth_image_view_;

    vk::CommandPool graphics_command_pool_;
    vk::Queue graphics_queue_;
    vk::Queue present_queue_;

    vk::CommandPool transient_command_pool_;
    vk::Queue transfer_queue_;

    TextureCache texture_cache_;
    MaterialCache material_cache_;

    vk::RenderPass render_pass_;

    vk::DescriptorSetLayout descriptor_set_layout_;

    vk::SampleCountFlagBits msaa_samples_ = vk::SampleCountFlagBits::e1;
};