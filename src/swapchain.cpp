#include "swapchain.h"

#include "renderer_state.h"
#include "utils.h"

Swapchain::Swapchain(RendererState& renderer, GLFWwindow* window)
    : device_(renderer.GetDevice())
{
    CreateSwapchain(renderer, window);
    CreateSwapchainImageViews(renderer);
}

Swapchain::~Swapchain() { Cleanup(); }

void Swapchain::RecreateSwapchain(RendererState& renderer, GLFWwindow* window)
{
    Cleanup();

    CreateSwapchain(renderer, window);
    CreateSwapchainImageViews(renderer);
}

vk::SwapchainKHR Swapchain::GetSwapchain() { return swapchain_; }

uint32_t Swapchain::GetMinimumImageCount() { return min_image_count_; }

uint32_t Swapchain::GetActualImageCount() { return image_count_; }

std::vector<vk::Image>& Swapchain::GetImages() { return swapchain_images_; }

vk::SurfaceFormatKHR& Swapchain::GetImageFormat()
{
    return swapchain_image_format_;
}

vk::Extent2D& Swapchain::GetExtent() { return swapchain_extent_; }

std::vector<vk::ImageView>& Swapchain::GetImageViews()
{
    return swapchain_image_views_;
}

void Swapchain::Cleanup()
{
    // for (auto framebuffer : swapchain_frame_buffers_) {
    //     device_.destroyFramebuffer(framebuffer);
    // }
    for (auto image_view : swapchain_image_views_) {
        device_.destroyImageView(image_view);
    }
    device_.destroySwapchainKHR(swapchain_);
}

vk::SurfaceFormatKHR Swapchain::ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& available_formats)
{
    for (const auto& format : available_formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }

    return available_formats[0];
}

vk::PresentModeKHR Swapchain::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& available_present_modes)
{
    for (const auto& present_mode : available_present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
            return present_mode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Swapchain::ChooseSwapExtent(
    GLFWwindow* window, const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        vk::Extent2D actual_extent = {static_cast<uint32_t>(width),
                                      static_cast<uint32_t>(height)};

        actual_extent.width =
            std::clamp(actual_extent.width, capabilities.minImageExtent.width,
                       capabilities.maxImageExtent.width);
        actual_extent.height =
            std::clamp(actual_extent.height, capabilities.minImageExtent.height,
                       capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

void Swapchain::CreateSwapchain(RendererState& renderer, GLFWwindow* window)
{
    auto details = renderer.QuerySwapChainSupport(renderer.GetPhysicalDevice());

    auto surface_format = ChooseSwapSurfaceFormat(details.formats);
    auto present_mode = ChooseSwapPresentMode(details.present_modes);
    auto extent = ChooseSwapExtent(window, details.capabilities);

    min_image_count_ = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0) {
        min_image_count_ =
            std::min(min_image_count_, details.capabilities.maxImageCount);
    }

    auto indices = renderer.GetQueueFamilies();
    uint32_t queue_family_indices[] = {indices.graphics_family.value().index,
                                       indices.present_family.value().index};

    vk::SharingMode sharing_mode = vk::SharingMode::eExclusive;
    uint32_t queue_family_index_count = 0;
    const uint32_t* queue_family_indices_arg = nullptr;

    if (indices.graphics_family.value().index !=
        indices.present_family.value().index) {
        sharing_mode = vk::SharingMode::eConcurrent;
        queue_family_index_count = 2;
        queue_family_indices_arg = queue_family_indices;
    }

    vk::SwapchainCreateInfoKHR swap_chain_info(
        vk::SwapchainCreateFlagsKHR(), renderer.GetSurface(), min_image_count_,
        surface_format.format, surface_format.colorSpace, extent, 1,
        vk::ImageUsageFlagBits::eColorAttachment, sharing_mode,
        queue_family_index_count, queue_family_indices_arg,
        details.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, present_mode, VK_TRUE);

    swapchain_ = renderer.GetDevice().createSwapchainKHR(swap_chain_info);
    swapchain_images_ = renderer.GetDevice().getSwapchainImagesKHR(swapchain_);
    image_count_ = swapchain_images_.size();
    swapchain_image_format_ = surface_format;
    swapchain_extent_ = extent;
}

vk::ResultValue<uint32_t> Swapchain::GetNextImage(uint64_t timeout,
                                                  vk::Semaphore semaphore,
                                                  vk::Fence fence)
{
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    try {
        result =
            device_.acquireNextImageKHR(swapchain_, timeout, semaphore, {});
    } catch (vk::SystemError& e) {
        if (e.code() == vk::Result::eErrorOutOfDateKHR) {
            result = {vk::Result::eErrorOutOfDateKHR, 0};
        } else {
            throw;
        }
    }
    return result;
}

void Swapchain::CreateSwapchainImageViews(RendererState& renderer)
{
    swapchain_image_views_.resize(image_count_);

    for (size_t i = 0; i < image_count_; ++i) {
        swapchain_image_views_[i] = CreateImageView(
            renderer, swapchain_images_[i], swapchain_image_format_.format,
            vk::ImageAspectFlagBits::eColor, 1);
    }
}