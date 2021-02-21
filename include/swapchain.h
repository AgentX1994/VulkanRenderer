#pragma once

#include <vector>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class RendererState;

class Swapchain
{
public:
    Swapchain(RendererState& renderer, GLFWwindow* window);
    
    Swapchain(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    ~Swapchain();

    void RecreateSwapchain(RendererState& renderer, GLFWwindow* window);

    vk::SwapchainKHR GetSwapchain();

    uint32_t GetMinimumImageCount();
    uint32_t GetActualImageCount();

    std::vector<vk::Image>& GetImages();
    vk::SurfaceFormatKHR& GetImageFormat();
    vk::Extent2D& GetExtent();
    std::vector<vk::ImageView>& GetImageViews();

    vk::ResultValue<uint32_t> GetNextImage(uint64_t timeout, vk::Semaphore semaphore, vk::Fence fence);

private:
    void Cleanup();

    vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
        const std::vector<vk::SurfaceFormatKHR>& available_formats);

    vk::PresentModeKHR ChooseSwapPresentMode(
        const std::vector<vk::PresentModeKHR>& available_present_modes);

    vk::Extent2D ChooseSwapExtent(
        GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities);

    void CreateSwapchain(RendererState& renderer, GLFWwindow* window);

    void CreateSwapchainImageViews(RendererState& renderer);

    vk::Device& device_;
    vk::SwapchainKHR swapchain_;
    uint32_t min_image_count_;
    uint32_t image_count_;
    std::vector<vk::Image> swapchain_images_;
    vk::SurfaceFormatKHR swapchain_image_format_;
    vk::Extent2D swapchain_extent_;
    std::vector<vk::ImageView> swapchain_image_views_;
    //std::vector<vk::Framebuffer> swapchain_frame_buffers_;
};