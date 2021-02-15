#include "application.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "stb_image.h"
#include "tiny_obj_loader.h"
#include "utils.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

vk::VertexInputBindingDescription Vertex::GetBindingDescription()
{
    return vk::VertexInputBindingDescription(0, sizeof(Vertex),
                                             vk::VertexInputRate::eVertex);
}

std::array<vk::VertexInputAttributeDescription, 3>
Vertex::GetAttributeDescriptions()
{
    return {vk::VertexInputAttributeDescription(
                0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(
                1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                offsetof(Vertex, tex_coord))};
}

bool Vertex::operator==(const Vertex& other) const
{
    return pos == other.pos && color == other.color &&
           tex_coord == other.tex_coord;
}

namespace std {
template <>
struct hash<Vertex>
{
    size_t operator()(const Vertex& vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos) ^
                 (hash<glm::vec3>()(vertex.color) << 1)) >>
                1) ^
               (hash<glm::vec2>()(vertex.tex_coord) << 1);
    }
};
}  // namespace std

struct UniformBufferObject
{
    alignas(4 * sizeof(float)) glm::mat4 model;
    alignas(4 * sizeof(float)) glm::mat4 view;
    alignas(4 * sizeof(float)) glm::mat4 proj;
};

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"};

// const std::vector<Vertex> TRIANGLE = {
//     { { 0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
//     { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
//     { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } }
// };

// const std::vector<Vertex> SQUARE_VERTICES = {
//     { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
//     { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
//     { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
//     { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
// };

// const std::vector<uint16_t> SQUARE_INDICES = {
//     0, 1, 2, 2, 3, 0
// };

// const std::vector<Vertex> TWO_SQUARES_VERTICES = {
//     { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
//     { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
//     { { 0.5f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
//     { { -0.5f, 0.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },

//     { { -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
//     { { 0.5f, -0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
//     { { 0.5f, 0.5f, -0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
//     { { -0.5f, 0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
// };

// const std::vector<uint16_t> TWO_SQUARES_INDICES = {
//     0, 1, 2, 2, 3, 0,
//     4, 5, 6, 6, 7, 4
// };

#ifndef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#endif

static VKAPI_ATTR uint32_t VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Application::Run()
{
    InitWindow();
    InitVulkan();
    SetupImgui();
    MainLoop();
    Cleanup();
}

void Application::SetFramebufferResized() { framebuffer_resized_ = true; }

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    window_ =
        glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void Application::InitVulkan()
{
    CreateInstance();
    // SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateCommandPool();
    CreateDescriptorSetLayout();
    CreateColorResources();
    CreateDepthResources();
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
    LoadModel();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
}

void Application::MainLoop()
{
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        DrawFrame();
    }

    logical_device_.waitIdle();
}

void Application::Cleanup()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupSwapChain();

    logical_device_.destroyCommandPool(imgui_command_pool_);
    logical_device_.destroyRenderPass(imgui_render_pass_);
    logical_device_.destroyDescriptorPool(imgui_descriptor_pool_);

    logical_device_.destroySampler(texture_sampler_);
    logical_device_.destroyImageView(texture_image_view_);
    logical_device_.destroyImage(texture_image_);
    logical_device_.freeMemory(texture_image_memory_);

    logical_device_.destroyDescriptorSetLayout(descriptor_set_layout_);

    logical_device_.destroyBuffer(index_buffer_);
    logical_device_.freeMemory(index_buffer_memory_);

    logical_device_.destroyBuffer(vertex_buffer_);
    logical_device_.freeMemory(vertex_buffer_memory_);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        logical_device_.destroySemaphore(render_finished_semaphore_[i]);
        logical_device_.destroySemaphore(image_available_semaphore_[i]);
        logical_device_.destroyFence(in_flight_fences_[i]);
    }
    logical_device_.destroyCommandPool(command_pool_);
    instance_.destroySurfaceKHR(surface_);
    logical_device_.destroy();
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        // instance_.destroyDebugUtilsMessengerEXT(debug_messenger_);
    }
    instance_.destroy();

    glfwDestroyWindow(window_);
    glfwTerminate();
}

const bool CheckExtensions(
    const std::vector<vk::ExtensionProperties> supported_extensions,
    std::vector<const char*> required_extensions)
{
    for (const auto extension_name : required_extensions) {
        if (std::find_if(supported_extensions.begin(),
                         supported_extensions.end(), [&extension_name](auto e) {
                             return strcmp(e.extensionName, extension_name) ==
                                    0;
                         }) == supported_extensions.end()) {
            return false;
        }
    }
    return true;
}

void PopulateDebugInfo(vk::DebugUtilsMessengerCreateInfoEXT& messenger_info)
{
    messenger_info.setMessageSeverity(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose);
    messenger_info.setMessageType(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    messenger_info.setPfnUserCallback(DebugCallback);
    messenger_info.setPUserData(nullptr);
}

void Application::CreateInstance()
{
    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        if (!CheckValidationLayerSupport()) {
            throw std::runtime_error("Not all validation layers supported!");
        }
    }

    vk::ApplicationInfo info("Hello Triangle", VK_MAKE_VERSION(1, 0, 0),
                             "No Engine", VK_MAKE_VERSION(1, 0, 0),
                             VK_API_VERSION_1_2);

    auto extensions = vk::enumerateInstanceExtensionProperties();
    auto required_extensions = GetRequiredExtensions();

    std::cout << "Available extensions:\n";
    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    std::cout << '\n';
    std::cout << "Required extensions:\n";
    for (const auto& extension : required_extensions) {
        std::cout << '\t' << extension << '\n';
    }

    if (!CheckExtensions(extensions, required_extensions)) {
        throw std::runtime_error("Not all required extensions are available!");
    }

    std::vector<const char*> layers;
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        layers = VALIDATION_LAYERS;
    }

    vk::InstanceCreateInfo create_info(vk::InstanceCreateFlags(), &info, layers,
                                       required_extensions);
    vk::DebugUtilsMessengerCreateInfoEXT messenger_info;
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        PopulateDebugInfo(messenger_info);
        create_info.setPNext(&messenger_info);
    }
    instance_ = vk::createInstance(create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance_);
}

bool Application::CheckValidationLayerSupport()
{
    auto validation_layers = vk::enumerateInstanceLayerProperties();

    for (const char* layer : VALIDATION_LAYERS) {
        if (std::find_if(validation_layers.begin(), validation_layers.end(),
                         [&layer](auto l) {
                             return strcmp(l.layerName, layer) == 0;
                         }) == validation_layers.end()) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> Application::GetRequiredExtensions()
{
    uint32_t glfw_required_extension_count;
    const char** glfw_required_extensions =
        glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

    std::vector<const char*> extensions(
        glfw_required_extensions,
        glfw_required_extensions + glfw_required_extension_count);

    if constexpr (ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Application::SetupDebugMessenger()
{
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        vk::DebugUtilsMessengerCreateInfoEXT messenger_info;
        PopulateDebugInfo(messenger_info);
        debug_messenger_ =
            instance_.createDebugUtilsMessengerEXT(messenger_info, nullptr);
    }
}

QueueFamilyIndices Application::FindQueueFamilies(
    const vk::PhysicalDevice& device)
{
    QueueFamilyIndices indices;

    auto queue_families = device.getQueueFamilyProperties();

    uint32_t i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = {i, queue_family};
        }
        if (device.getSurfaceSupportKHR(i, surface_)) {
            indices.present_family = {i, queue_family};
        }

        if (indices.IsComplete()) {
            break;
        }

        ++i;
    }

    return indices;
}

bool Application::CheckDeviceExtensionSupport(const vk::PhysicalDevice& device)
{
    auto available_extensions = device.enumerateDeviceExtensionProperties();

    for (const auto& extension : DEVICE_EXTENSIONS) {
        if (std::find_if(available_extensions.begin(),
                         available_extensions.end(), [&extension](auto e) {
                             return strcmp(e.extensionName, extension) == 0;
                         }) == available_extensions.end()) {
            return false;
        }
    }

    return true;
}

SwapChainSupportDetails Application::QuerySwapChainSupport(
    const vk::PhysicalDevice& device)
{
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
    details.formats = device.getSurfaceFormatsKHR(surface_);
    details.present_modes = device.getSurfacePresentModesKHR(surface_);

    return details;
}

bool Application::IsDeviceSuitable(const vk::PhysicalDevice& device)
{
    auto indices = FindQueueFamilies(device);
    bool extensions_supported = CheckDeviceExtensionSupport(device);
    bool swapchain_adequate = false;
    if (extensions_supported) {
        auto details = QuerySwapChainSupport(device);
        swapchain_adequate =
            !details.formats.empty() && !details.present_modes.empty();
    }
    auto supported_features = device.getFeatures();
    return indices.IsComplete() && extensions_supported && swapchain_adequate &&
           supported_features.samplerAnisotropy;
}

void Application::PickPhysicalDevice()
{
    auto physical_devices = instance_.enumeratePhysicalDevices();
    if (physical_devices.size() == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto& device : physical_devices) {
        if (IsDeviceSuitable(device)) {
            physical_device_ = device;
            break;
        }
    }

    if (!physical_device_) {
        throw std::runtime_error("failed to find a suitable GPU");
    }

    msaa_samples_ = GetMaxUsableSampleCount();
    vk::PhysicalDeviceProperties properties = physical_device_.getProperties();
    std::cout << "Found device:\n";
    std::cout << '\t' << properties.deviceName << '\n';
}

void Application::CreateLogicalDevice()
{
    auto indices = FindQueueFamilies(physical_device_);

    float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos = {
        {vk::DeviceQueueCreateFlags(), indices.graphics_family.value().index, 1,
         &priority},  // Graphics queue
        {vk::DeviceQueueCreateFlags(), indices.present_family.value().index, 1,
         &priority}  // Present queue
    };

    // If the queues are the same, just request two of the same queue
    if (indices.graphics_family.value().index ==
        indices.present_family.value().index) {
        queue_create_infos.pop_back();
        if (indices.graphics_family.value().properties.queueCount > 1) {
            queue_create_infos[0].queueCount += 1;
        }
    }

    vk::PhysicalDeviceFeatures device_features;
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.sampleRateShading = VK_TRUE;

    vk::DeviceCreateInfo create_info;
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pEnabledFeatures = &device_features;
    create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    create_info.enabledExtensionCount = DEVICE_EXTENSIONS.size();
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        create_info.enabledLayerCount = VALIDATION_LAYERS.size();
    }

    logical_device_ = physical_device_.createDevice(create_info);

    graphics_queue_ =
        logical_device_.getQueue(indices.graphics_family.value().index, 0);
    if (indices.graphics_family.value().index ==
            indices.present_family.value().index &&
        indices.graphics_family.value().properties.queueCount > 1) {
        present_queue_ =
            logical_device_.getQueue(indices.present_family.value().index, 1);
    } else {
        present_queue_ =
            logical_device_.getQueue(indices.present_family.value().index, 0);
    }
    VULKAN_HPP_DEFAULT_DISPATCHER.init(logical_device_);
}

void Application::CreateSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = surface;
}

vk::SurfaceFormatKHR Application::ChooseSwapSurfaceFormat(
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

vk::PresentModeKHR Application::ChooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& available_present_modes)
{
    for (const auto& present_mode : available_present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
            return present_mode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Application::ChooseSwapExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);

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

void Application::CreateSwapChain()
{
    SwapChainSupportDetails details = QuerySwapChainSupport(physical_device_);

    auto surface_format = ChooseSwapSurfaceFormat(details.formats);
    auto present_mode = ChooseSwapPresentMode(details.present_modes);
    auto extent = ChooseSwapExtent(details.capabilities);

    min_image_count_ = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0) {
        min_image_count_ =
            std::min(image_count_, details.capabilities.maxImageCount);
    }

    auto indices = FindQueueFamilies(physical_device_);
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
        vk::SwapchainCreateFlagsKHR(), surface_, min_image_count_,
        surface_format.format, surface_format.colorSpace, extent, 1,
        vk::ImageUsageFlagBits::eColorAttachment, sharing_mode,
        queue_family_index_count, queue_family_indices_arg,
        details.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, present_mode, VK_TRUE);

    swapchain_ = logical_device_.createSwapchainKHR(swap_chain_info);
    swap_chain_images_ = logical_device_.getSwapchainImagesKHR(swapchain_);
    image_count_ = swap_chain_images_.size();
    swap_chain_image_format_ = surface_format.format;
    swap_chain_extent_ = extent;
}

void Application::CreateRenderPass()
{
    vk::AttachmentDescription color_attachment(
        vk::AttachmentDescriptionFlags(), swap_chain_image_format_,
        msaa_samples_, vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depth_attachment(
        vk::AttachmentDescriptionFlags(), FindDepthFormat(), msaa_samples_,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentDescription color_attachment_resolve(
        vk::AttachmentDescriptionFlags(), swap_chain_image_format_,
        vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference color_attachment_ref(
        0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference depth_attachment_ref(
        1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference color_attachment_resolve_ref(
        2, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(
        vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, {},
        color_attachment_ref, color_attachment_resolve_ref,
        &depth_attachment_ref, {});

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL, 0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::PipelineStageFlagBits::eColorAttachmentOutput |
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::AccessFlagBits{0},
        vk::AccessFlagBits::eColorAttachmentWrite |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    std::array<vk::AttachmentDescription, 3> attachments = {
        {color_attachment, depth_attachment, color_attachment_resolve}};

    vk::RenderPassCreateInfo render_pass_info(vk::RenderPassCreateFlags(),
                                              attachments, subpass, dependency);

    render_pass_ = logical_device_.createRenderPass(render_pass_info);
}

vk::ImageView Application::CreateImageView(vk::Image image, vk::Format format,
                                           vk::ImageAspectFlags aspect_flags,
                                           uint32_t mip_levels)
{
    vk::ImageViewCreateInfo view_info(
        vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, format,
        vk::ComponentSwizzle(),
        vk::ImageSubresourceRange(aspect_flags, 0, mip_levels, 0, 1));

    return logical_device_.createImageView(view_info);
}

void Application::CreateImageViews()
{
    swap_chain_image_views_.resize(image_count_);

    for (size_t i = 0; i < image_count_; ++i) {
        swap_chain_image_views_[i] =
            CreateImageView(swap_chain_images_[i], swap_chain_image_format_,
                            vk::ImageAspectFlagBits::eColor, 1);
    }
}

void Application::CreateGraphicsPipeline()
{
    auto vert_shader_bin =
        CompileShader("shaders/shader.vert", shaderc_glsl_vertex_shader);
    auto frag_shader_bin =
        CompileShader("shaders/shader.frag", shaderc_glsl_fragment_shader);

    auto vert_shader_module = CreateShaderModule(vert_shader_bin);
    auto frag_shader_module = CreateShaderModule(frag_shader_bin);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex,
        vert_shader_module, "main");
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment, frag_shader_module, "main");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
        vert_shader_stage_info, frag_shader_stage_info};

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input_info(
        vk::PipelineVertexInputStateCreateFlags(),
        bindingDescription,    // Vertex Binding Descriptions
        attributeDescriptions  // Vertex attribute descriptions
    );

    vk::PipelineInputAssemblyStateCreateInfo input_assembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    vk::Viewport viewport(0.0f, 0.0f, (float)swap_chain_extent_.width,
                          (float)swap_chain_extent_.height, 0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, swap_chain_extent_);

    vk::PipelineViewportStateCreateInfo viewport_state(
        vk::PipelineViewportStateCreateFlags(), viewport, scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        vk::PipelineRasterizationStateCreateFlags(), VK_FALSE, VK_FALSE,
        vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        vk::PipelineMultisampleStateCreateFlags(), msaa_samples_, VK_TRUE, 0.2f,
        nullptr, VK_FALSE, VK_FALSE);

    vk::PipelineColorBlendAttachmentState color_blend_attachment(
        VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo color_blending(
        vk::PipelineColorBlendStateCreateFlags(), VK_FALSE, vk::LogicOp::eCopy,
        color_blend_attachment, {0.0f, 0.0f, 0.0f, 0.0f});

    std::vector<vk::DynamicState> dynamic_states = {
        // vk::DynamicState::eViewport,
        // vk::DynamicState::eLineWidth
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state(
        vk::PipelineDynamicStateCreateFlags(), dynamic_states);

    vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(),
        descriptor_set_layout_,  // Set layouts
        {}                       // Push constant ranges
    );

    pipeline_layout_ =
        logical_device_.createPipelineLayout(pipeline_layout_info);

    vk::PipelineDepthStencilStateCreateInfo depth_stencil(
        vk::PipelineDepthStencilStateCreateFlags(), VK_TRUE, VK_TRUE,
        vk::CompareOp::eLess, VK_FALSE, VK_FALSE, {}, {});

    vk::GraphicsPipelineCreateInfo pipeline_info(
        vk::PipelineCreateFlags(), shader_stages, &vertex_input_info,
        &input_assembly, {}, &viewport_state, &rasterizer, &multisampling,
        &depth_stencil, &color_blending, &dynamic_state, pipeline_layout_,
        render_pass_, 0, {}, -1);

    auto result = logical_device_.createGraphicsPipeline({}, pipeline_info);
    switch (result.result) {
        case vk::Result::eSuccess:
            graphics_pipeline_ = result.value;
            break;
        case vk::Result::ePipelineCompileRequiredEXT:
            throw std::runtime_error("Pipeline compile required (WTF?)");
    }

    logical_device_.destroyShaderModule(frag_shader_module);
    logical_device_.destroyShaderModule(vert_shader_module);
}

vk::ShaderModule Application::CreateShaderModule(
    const std::vector<uint32_t>& code)
{
    vk::ShaderModuleCreateInfo create_info(vk::ShaderModuleCreateFlagBits(),
                                           code);

    return logical_device_.createShaderModule(create_info);
}

void Application::CreateFramebuffers()
{
    swap_chain_frame_buffers_.resize(image_count_);

    for (size_t i = 0; i < swap_chain_image_views_.size(); ++i) {
        std::array<vk::ImageView, 3> attachments = {
            color_image_view_, depth_image_view_, swap_chain_image_views_[i]};

        vk::FramebufferCreateInfo framebuffer_info(
            vk::FramebufferCreateFlags(), render_pass_, attachments,
            swap_chain_extent_.width, swap_chain_extent_.height, 1);

        swap_chain_frame_buffers_[i] =
            logical_device_.createFramebuffer(framebuffer_info);
    }
}

void Application::CreateCommandPool()
{
    auto indices = FindQueueFamilies(physical_device_);

    vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlags(),
                                        indices.graphics_family.value().index);

    command_pool_ = logical_device_.createCommandPool(pool_info);
}

std::pair<vk::Buffer, vk::DeviceMemory> Application::CreateBuffer(
    vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo buffer_info(vk::BufferCreateFlags(), size, usage,
                                     vk::SharingMode::eExclusive);

    auto buffer = logical_device_.createBuffer(buffer_info);

    auto mem_reqs = logical_device_.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size, FindMemoryType(mem_reqs.memoryTypeBits, properties));

    auto memory = logical_device_.allocateMemory(alloc_info);
    logical_device_.bindBufferMemory(buffer, memory, 0);

    return {buffer, memory};
}

void Application::CreateVertexBuffer()
{
    vk::DeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

    auto [staging_buffer, staging_buffer_memory] =
        CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data =
        logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, vertices.data(), (size_t)buffer_size);
    logical_device_.unmapMemory(staging_buffer_memory);

    std::tie(vertex_buffer_, vertex_buffer_memory_) =
        CreateBuffer(buffer_size,
                     vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eVertexBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal);

    CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

void Application::CreateIndexBuffer()
{
    vk::DeviceSize buffer_size = sizeof(indices[0]) * indices.size();

    auto [staging_buffer, staging_buffer_memory] =
        CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data =
        logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, indices.data(), (size_t)buffer_size);
    logical_device_.unmapMemory(staging_buffer_memory);

    std::tie(index_buffer_, index_buffer_memory_) =
        CreateBuffer(buffer_size,
                     vk::BufferUsageFlagBits::eTransferDst |
                         vk::BufferUsageFlagBits::eIndexBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal);

    CopyBuffer(staging_buffer, index_buffer_, buffer_size);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

void Application::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo alloc_info(command_pool_,
                                             vk::CommandBufferLevel::ePrimary,
                                             swap_chain_frame_buffers_.size());

    command_buffers_ = logical_device_.allocateCommandBuffers(alloc_info);

    for (size_t i = 0; i < command_buffers_.size(); ++i) {
        vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlags(),
                                              {});

        command_buffers_[i].begin(begin_info);

        std::array<vk::ClearValue, 2> clear_values;
        clear_values[0].color.setFloat32({{0.0f, 0.0f, 0.0f, 0.0f}});
        clear_values[1].depthStencil.setDepth(1.0f);
        clear_values[1].depthStencil.setStencil(0);

        vk::RenderPassBeginInfo render_pass_info(
            render_pass_, swap_chain_frame_buffers_[i],
            {{0, 0}, swap_chain_extent_}, clear_values);

        command_buffers_[i].beginRenderPass(render_pass_info,
                                            vk::SubpassContents::eInline);

        command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                         graphics_pipeline_);

        command_buffers_[i].bindVertexBuffers(0, vertex_buffer_, {0});
        command_buffers_[i].bindIndexBuffer(index_buffer_, 0,
                                            vk::IndexType::eUint32);
        command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                               pipeline_layout_, 0,
                                               descriptor_sets_[i], {});
        command_buffers_[i].drawIndexed(static_cast<uint32_t>(indices.size()),
                                        1, 0, 0, 0);

        command_buffers_[i].endRenderPass();
        command_buffers_[i].end();
    }
}

void Application::CreateSyncObjects()
{
    // Create these as empty (default) so that we can copy in_flight fences into
    // them
    images_in_flight_.resize(image_count_);

    vk::SemaphoreCreateInfo semaphore_info;
    vk::FenceCreateInfo fence_info(
        vk::FenceCreateFlagBits::eSignaled);  // Create signaled so we don't get
                                              // stuck waiting for it

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        image_available_semaphore_[i] =
            logical_device_.createSemaphore(semaphore_info);
        render_finished_semaphore_[i] =
            logical_device_.createSemaphore(semaphore_info);
        in_flight_fences_[i] = logical_device_.createFence(fence_info);
    }
}

void Application::DrawFrame()
{
    // Wait until this fence has been finished
    auto wait_for_fence_result = logical_device_.waitForFences(
        in_flight_fences_[current_frame_], VK_TRUE,
        std::numeric_limits<uint64_t>::max());
    if (wait_for_fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("Could not wait for fence!");
    }

    // Get next image
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    try {
        result = logical_device_.acquireNextImageKHR(
            swapchain_, std::numeric_limits<uint64_t>::max(),
            image_available_semaphore_[current_frame_], {});
    } catch (vk::SystemError& e) {
        if (e.code() == vk::Result::eErrorOutOfDateKHR) {
            RecreateSwapChain();
            return;
        } else {
            throw;
        }
    }
    uint32_t image_index;
    switch (result.result) {
        case vk::Result::eSuccess:
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eNotReady:
            image_index = result.value;
            break;
        case vk::Result::eTimeout:
            throw std::runtime_error("Could not acquire next image!");
    }

    // Check if a previous frame is using this image
    // operator bool() is true if not VK_NULL_HANDLE
    if (images_in_flight_[image_index]) {
        auto wait_for_image_fence = logical_device_.waitForFences(
            images_in_flight_[image_index], VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        if (wait_for_image_fence != vk::Result::eSuccess) {
            throw std::runtime_error("Could not wait for image fence!");
        }
    }

    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    UpdateUniformBuffer(image_index);

    vk::PipelineStageFlags wait_dest_stage_mask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowMetricsWindow(&imgui_display_);
    ImGui::Render();
    {
        vk::CommandBufferBeginInfo begin_info(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        );
        imgui_command_buffers_[image_index].begin(begin_info);
        vk::ClearValue clear_value;
        clear_value.color.setFloat32({{0.0f, 0.0f, 0.0f, 1.0f}});
        vk::RenderPassBeginInfo imgui_pass(
            imgui_render_pass_, imgui_frame_buffers_[image_index],
            {{0, 0}, swap_chain_extent_}, clear_value);
        imgui_command_buffers_[image_index].beginRenderPass(
            imgui_pass, vk::SubpassContents::eInline);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                        imgui_command_buffers_[image_index]);
        imgui_command_buffers_[image_index].endRenderPass();
        imgui_command_buffers_[image_index].end();
    }

    std::array<vk::CommandBuffer, 2> command_buffers_to_submit = {
        {command_buffers_[image_index], imgui_command_buffers_[image_index]}};

    vk::SubmitInfo submit_info(image_available_semaphore_[current_frame_],
                               wait_dest_stage_mask, command_buffers_to_submit,
                               render_finished_semaphore_[current_frame_]);

    logical_device_.resetFences(in_flight_fences_[current_frame_]);

    graphics_queue_.submit(submit_info, in_flight_fences_[current_frame_]);

    vk::PresentInfoKHR present_info(render_finished_semaphore_[current_frame_],
                                    swapchain_, image_index, {});

    vk::Result present_result = vk::Result::eSuccess;
    try {
        present_result = present_queue_.presentKHR(present_info);
    } catch (vk::SystemError& e) {
        if (e.code() == vk::Result::eErrorOutOfDateKHR) {
            // this will recreate the swapchain below
            present_result = vk::Result::eErrorOutOfDateKHR;
        } else {
            throw;
        }
    }

    if (present_result == vk::Result::eSuboptimalKHR ||
        present_result == vk::Result::eErrorOutOfDateKHR ||
        framebuffer_resized_) {
        framebuffer_resized_ = false;
        RecreateSwapChain();
    } else if (present_result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::CleanupSwapChain()
{
    for (auto framebuffer : imgui_frame_buffers_) {
        logical_device_.destroyFramebuffer(framebuffer);
    }
    logical_device_.freeCommandBuffers(imgui_command_pool_,
                                       imgui_command_buffers_);

    logical_device_.destroyImageView(color_image_view_);
    logical_device_.destroyImage(color_image_);
    logical_device_.freeMemory(color_image_memory_);

    logical_device_.destroyImageView(depth_image_view_);
    logical_device_.destroyImage(depth_image_);
    logical_device_.freeMemory(depth_image_memory_);

    for (size_t i = 0; i < image_count_; ++i) {
        logical_device_.destroyBuffer(uniform_buffers_[i]);
        logical_device_.freeMemory(uniform_buffers_memory_[i]);
    }

    logical_device_.destroyDescriptorPool(descriptor_pool_);

    for (auto framebuffer : swap_chain_frame_buffers_) {
        logical_device_.destroyFramebuffer(framebuffer);
    }
    logical_device_.freeCommandBuffers(command_pool_, command_buffers_);

    logical_device_.destroyPipeline(graphics_pipeline_);
    logical_device_.destroyPipelineLayout(pipeline_layout_);
    logical_device_.destroyRenderPass(render_pass_);
    for (auto image_view : swap_chain_image_views_) {
        logical_device_.destroyImageView(image_view);
    }
    logical_device_.destroySwapchainKHR(swapchain_);
}

void Application::RecreateSwapChain()
{
    // handle minimization
    int width = 0, height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    logical_device_.waitIdle();

    CleanupSwapChain();

    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateColorResources();
    CreateDepthResources();
    CreateFramebuffers();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();

    ImGui_ImplVulkan_SetMinImageCount(min_image_count_);

    vk::CommandBufferAllocateInfo command_buffer_info(
        imgui_command_pool_, vk::CommandBufferLevel::ePrimary, image_count_);
    imgui_command_buffers_ =
        logical_device_.allocateCommandBuffers(command_buffer_info);
    imgui_frame_buffers_.resize(image_count_);
    for (uint32_t i = 0; i < image_count_; ++i) {
        vk::FramebufferCreateInfo info(
            vk::FramebufferCreateFlags(), imgui_render_pass_,
            swap_chain_image_views_[i], swap_chain_extent_.width,
            swap_chain_extent_.height, 1);
        imgui_frame_buffers_[i] = logical_device_.createFramebuffer(info);
    }
}

uint32_t Application::FindMemoryType(uint32_t type_filter,
                                     vk::MemoryPropertyFlags properties)
{
    auto mem_properties = physical_device_.getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Application::CopyBuffer(vk::Buffer source, vk::Buffer destination,
                             vk::DeviceSize size)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::BufferCopy copy_info(0, 0, size);

    command_buffer.copyBuffer(source, destination, copy_info);

    EndSingleTimeCommands(command_buffer);
}

void Application::CreateDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding ubo_layout_binding(
        0, vk::DescriptorType::eUniformBuffer, 1,
        vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding sampler_layout_binding(
        1, vk::DescriptorType::eCombinedImageSampler, 1,
        vk::ShaderStageFlagBits::eFragment);

std:
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
        ubo_layout_binding, sampler_layout_binding};

    vk::DescriptorSetLayoutCreateInfo layout_info(
        vk::DescriptorSetLayoutCreateFlags(), bindings);

    descriptor_set_layout_ =
        logical_device_.createDescriptorSetLayout(layout_info);
}

void Application::CreateUniformBuffers()
{
    vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

    uniform_buffers_.resize(image_count_);
    uniform_buffers_memory_.resize(image_count_);

    for (size_t i = 0; i < image_count_; ++i) {
        std::tie(uniform_buffers_[i], uniform_buffers_memory_[i]) =
            CreateBuffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible |
                             vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void Application::UpdateUniformBuffer(uint32_t index)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     current_time - start_time)
                     .count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f,
        10.0f);
    // compensate for incorect y coordinate in clipping space (OpenGL has it
    // flipped compared to Vulkan)
    ubo.proj[1][1] *= -1;

    void* data = logical_device_.mapMemory(uniform_buffers_memory_[index], 0,
                                           sizeof(ubo));
    memcpy(data, &ubo, sizeof(ubo));
    logical_device_.unmapMemory(uniform_buffers_memory_[index]);
}

void Application::CreateDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
        {{vk::DescriptorType::eUniformBuffer,
          static_cast<uint32_t>(image_count_)},
         {vk::DescriptorType::eCombinedImageSampler,
          static_cast<uint32_t>(image_count_)}}};

    vk::DescriptorPoolCreateInfo pool_info(vk::DescriptorPoolCreateFlags(),
                                           static_cast<uint32_t>(image_count_),
                                           pool_sizes);

    descriptor_pool_ = logical_device_.createDescriptorPool(pool_info);
}

void Application::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(image_count_,
                                                 descriptor_set_layout_);
    vk::DescriptorSetAllocateInfo alloc_info(descriptor_pool_, layouts);

    descriptor_sets_ = logical_device_.allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < image_count_; ++i) {
        vk::DescriptorBufferInfo buffer_info(uniform_buffers_[i], 0,
                                             sizeof(UniformBufferObject));

        vk::DescriptorImageInfo image_info(
            texture_sampler_, texture_image_view_,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        std::array<vk::WriteDescriptorSet, 2> descriptor_writes = {
            {{descriptor_sets_[i],
              0,
              0,
              vk::DescriptorType::eUniformBuffer,
              {},
              buffer_info},
             {descriptor_sets_[i], 1, 0,
              vk::DescriptorType::eCombinedImageSampler, image_info}}};

        logical_device_.updateDescriptorSets(descriptor_writes, {});
    }
}

std::pair<vk::Image, vk::DeviceMemory> Application::CreateImage(
    uint32_t width, uint32_t height, uint32_t mip_levels,
    vk::SampleCountFlagBits num_samples, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::DeviceSize image_size = width * height * 4;

    vk::ImageCreateInfo image_info(
        vk::ImageCreateFlags(), vk::ImageType::e2D, format, {width, height, 1},
        mip_levels, 1, num_samples, tiling, usage, vk::SharingMode::eExclusive,
        {}, vk::ImageLayout::eUndefined);

    vk::Image image = logical_device_.createImage(image_info);

    vk::MemoryRequirements mem_reqs =
        logical_device_.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size, FindMemoryType(mem_reqs.memoryTypeBits, properties));

    vk::DeviceMemory memory = logical_device_.allocateMemory(alloc_info);
    logical_device_.bindImageMemory(image, memory, 0);

    return {image, memory};
}

void Application::CreateTextureImage()
{
    int texture_width, texture_height, texture_channels;
    stbi_uc* pixels =
        stbi_load(TEXTURE_PATH.c_str(), &texture_width, &texture_height,
                  &texture_channels, STBI_rgb_alpha);
    vk::DeviceSize image_size = texture_width * texture_height * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image");
    }

    mip_levels_ = static_cast<uint32_t>(std::floor(
                      std::log2(std::max(texture_width, texture_height)))) +
                  1;

    auto [staging_buffer, staging_buffer_memory] =
        CreateBuffer(image_size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data =
        logical_device_.mapMemory(staging_buffer_memory, 0, image_size);
    memcpy(data, pixels, static_cast<size_t>(image_size));
    logical_device_.unmapMemory(staging_buffer_memory);

    stbi_image_free(pixels);

    std::tie(texture_image_, texture_image_memory_) = CreateImage(
        texture_width, texture_height, mip_levels_, vk::SampleCountFlagBits::e1,
        vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    TransitionImageLayout(texture_image_, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal, mip_levels_);

    CopyBufferToImage(staging_buffer, texture_image_, texture_width,
                      texture_height);

    // will transition while generating mipmaps
    //   TransitionImageLayout(texture_image_, vk::Format::eR8G8B8A8Srgb,
    //                         vk::ImageLayout::eTransferDstOptimal,
    //                         vk::ImageLayout::eShaderReadOnlyOptimal,
    //                         mip_levels_);
    GenerateMipMaps(texture_image_, vk::Format::eR8G8B8A8Srgb, texture_width,
                    texture_height, mip_levels_);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

vk::CommandBuffer Application::BeginSingleTimeCommands()
{
    // TODO Transient CommandPool?
    vk::CommandBufferAllocateInfo alloc_info(
        command_pool_, vk::CommandBufferLevel::ePrimary, 1);

    vk::CommandBuffer command_buffer =
        logical_device_.allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    command_buffer.begin(begin_info);
    return command_buffer;
}

void Application::EndSingleTimeCommands(vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    vk::SubmitInfo submit_info({}, {}, command_buffer, {});

    graphics_queue_.submit(submit_info);
    graphics_queue_.waitIdle();

    logical_device_.freeCommandBuffers(command_pool_, command_buffer);
}

void Application::TransitionImageLayout(vk::Image image, vk::Format format,
                                        vk::ImageLayout old_layout,
                                        vk::ImageLayout new_layout,
                                        uint32_t mip_levels)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::AccessFlags source_access_mask;
    vk::AccessFlags destination_access_mask;
    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined &&
        new_layout == vk::ImageLayout::eTransferDstOptimal) {
        source_access_mask = vk::AccessFlags(0);
        destination_access_mask = vk::AccessFlagBits::eTransferWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        source_access_mask = vk::AccessFlagBits::eTransferWrite;
        destination_access_mask = vk::AccessFlagBits::eShaderRead;
        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eUndefined &&
               new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        source_access_mask = vk::AccessFlags(0);
        destination_access_mask =
            vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vk::ImageMemoryBarrier barrier(
        source_access_mask, destination_access_mask, old_layout, new_layout,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0,
                                  mip_levels, 0, 1));

    if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if (HasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |=
                vk::ImageAspectFlagBits::eStencil;
        }
    }

    command_buffer.pipelineBarrier(source_stage, destination_stage,
                                   vk::DependencyFlags(), {}, {}, barrier);

    EndSingleTimeCommands(command_buffer);
}

void Application::CopyBufferToImage(vk::Buffer buffer, vk::Image image,
                                    uint32_t width, uint32_t height)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {width, height, 1});

    command_buffer.copyBufferToImage(
        buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    EndSingleTimeCommands(command_buffer);
}

void Application::CreateTextureImageView()
{
    texture_image_view_ =
        CreateImageView(texture_image_, vk::Format::eR8G8B8A8Srgb,
                        vk::ImageAspectFlagBits::eColor, mip_levels_);
}

void Application::CreateTextureSampler()
{
    auto properties = physical_device_.getProperties();
    vk::SamplerCreateInfo sampler_info(
        vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f,
        VK_TRUE, properties.limits.maxSamplerAnisotropy, VK_FALSE,
        vk::CompareOp::eAlways, 0.0f, static_cast<float>(mip_levels_),
        vk::BorderColor::eIntOpaqueBlack, VK_FALSE);

    texture_sampler_ = logical_device_.createSampler(sampler_info);
}

vk::Format Application::FindSupportedFormat(
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features)
{
    for (auto format : candidates) {
        auto props = physical_device_.getFormatProperties(format);
        switch (tiling) {
            case vk::ImageTiling::eLinear:
                if ((props.linearTilingFeatures & features) == features) {
                    return format;
                }
                break;
            case vk::ImageTiling::eOptimal:
                if ((props.optimalTilingFeatures & features) == features) {
                    return format;
                }
                break;
            case vk::ImageTiling::eDrmFormatModifierEXT:
                // TODO What is this
                break;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format Application::FindDepthFormat()
{
    return FindSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool Application::HasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint ||
           format == vk::Format::eD24UnormS8Uint;
}

void Application::CreateDepthResources()
{
    vk::Format depth_format = FindDepthFormat();

    std::tie(depth_image_, depth_image_memory_) =
        CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, 1,
                    msaa_samples_, depth_format, vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eDepthStencilAttachment,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);

    depth_image_view_ = CreateImageView(depth_image_, depth_format,
                                        vk::ImageAspectFlagBits::eDepth, 1);
    TransitionImageLayout(depth_image_, depth_format,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
}

void Application::LoadModel()
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                          MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> unique_vertices;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            // Currently assuming that every vertex is unique
            // TODO: Fix this
            Vertex vertex{};

            vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
                          attrib.vertices[3 * index.vertex_index + 1],
                          attrib.vertices[3 * index.vertex_index + 2]};

            vertex.tex_coord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                // the loader loads top to bottom, but Vulkan is bottom to top
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (unique_vertices.count(vertex) == 0) {
                unique_vertices[vertex] =
                    static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(unique_vertices[vertex]);
        }
    }
}

void Application::GenerateMipMaps(vk::Image image, vk::Format format,
                                  int32_t texture_width, int32_t texture_height,
                                  uint32_t mip_levels)
{
    auto format_properties = physical_device_.getFormatProperties(format);
    if (!(format_properties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error(
            "texture image format does not support linear blitting!");
    }

    auto command_buffer = BeginSingleTimeCommands();

    vk::ImageMemoryBarrier barrier(
        vk::AccessFlags(0), vk::AccessFlags(0), vk::ImageLayout::eUndefined,
        vk::ImageLayout::eUndefined, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    int32_t mip_width = texture_width;
    int32_t mip_height = texture_height;

    for (uint32_t i = 1; i < mip_levels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer, {},
                                       {}, {}, barrier);

        vk::ImageBlit blit;
        blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1] = vk::Offset3D(mip_width, mip_height, 1);
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1] =
            vk::Offset3D(mip_width > 1 ? mip_width / 2 : 1,
                         mip_height > 1 ? mip_height / 2 : 1, 1);
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        command_buffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
                                 image, vk::ImageLayout::eTransferDstOptimal,
                                 blit, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mip_width > 1) {
            mip_width /= 2;
        }
        if (mip_height > 1) {
            mip_height /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {}, {}, {}, barrier);

    EndSingleTimeCommands(command_buffer);
}

vk::SampleCountFlagBits Application::GetMaxUsableSampleCount()
{
    auto phys_dev_props = physical_device_.getProperties();

    vk::SampleCountFlags counts =
        phys_dev_props.limits.framebufferColorSampleCounts &
        phys_dev_props.limits.framebufferDepthSampleCounts;

    if (counts & vk::SampleCountFlagBits::e64) {
        return vk::SampleCountFlagBits::e64;
    }
    if (counts & vk::SampleCountFlagBits::e32) {
        return vk::SampleCountFlagBits::e32;
    }
    if (counts & vk::SampleCountFlagBits::e16) {
        return vk::SampleCountFlagBits::e16;
    }
    if (counts & vk::SampleCountFlagBits::e8) {
        return vk::SampleCountFlagBits::e8;
    }
    if (counts & vk::SampleCountFlagBits::e4) {
        return vk::SampleCountFlagBits::e4;
    }
    if (counts & vk::SampleCountFlagBits::e2) {
        return vk::SampleCountFlagBits::e2;
    }

    return vk::SampleCountFlagBits::e1;
}

void Application::CreateColorResources()
{
    vk::Format color_format = swap_chain_image_format_;

    std::tie(color_image_, color_image_memory_) =
        CreateImage(swap_chain_extent_.width, swap_chain_extent_.height, 1,
                    msaa_samples_, color_format, vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eTransientAttachment |
                        vk::ImageUsageFlagBits::eColorAttachment,
                    vk::MemoryPropertyFlagBits::eDeviceLocal);
    color_image_view_ = CreateImageView(color_image_, color_format,
                                        vk::ImageAspectFlagBits::eColor, 1);
}

static void check_vk_result(VkResult err)
{
    if (err == 0) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

void Application::SetupImgui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    // Create descriptor pool just for ImGui
    // not sure if this is necessary...
    {
        std::array<vk::DescriptorPoolSize, 11> pool_sizes = {
            {{vk::DescriptorType::eSampler, 1000u},
             {vk::DescriptorType::eCombinedImageSampler, 1000u},
             {vk::DescriptorType::eSampledImage, 1000u},
             {vk::DescriptorType::eStorageImage, 1000u},
             {vk::DescriptorType::eUniformTexelBuffer, 1000u},
             {vk::DescriptorType::eStorageTexelBuffer, 1000u},
             {vk::DescriptorType::eUniformBuffer, 1000u},
             {vk::DescriptorType::eStorageBuffer, 1000u},
             {vk::DescriptorType::eUniformBufferDynamic, 1000u},
             {vk::DescriptorType::eStorageBufferDynamic, 1000u},
             {vk::DescriptorType::eInputAttachment, 1000u}}};
        vk::DescriptorPoolCreateInfo pool_info(
            vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            static_cast<uint32_t>(1000 * 11), pool_sizes);
        imgui_descriptor_pool_ =
            logical_device_.createDescriptorPool(pool_info);
    }

    // Create imgui render pass
    {
        vk::AttachmentDescription attachment(
            vk::AttachmentDescriptionFlags(), swap_chain_image_format_,
            vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::ePresentSrcKHR);

        vk::AttachmentReference color_attachment(
            0, vk::ImageLayout::eColorAttachmentOptimal);

        vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(),
                                       vk::PipelineBindPoint::eGraphics, {},
                                       color_attachment);

        vk::SubpassDependency dependency(
            VK_SUBPASS_EXTERNAL, 0,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits{0}, vk::AccessFlagBits::eColorAttachmentWrite);

        vk::RenderPassCreateInfo render_pass_info(
            vk::RenderPassCreateFlags(), attachment, subpass, dependency);

        imgui_render_pass_ = logical_device_.createRenderPass(render_pass_info);
    }

    // create command pool and buffers
    {
        vk::CommandPoolCreateInfo command_pool_info(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            FindQueueFamilies(physical_device_).graphics_family.value().index);
        imgui_command_pool_ =
            logical_device_.createCommandPool(command_pool_info);

        vk::CommandBufferAllocateInfo command_buffer_info(
            imgui_command_pool_, vk::CommandBufferLevel::ePrimary,
            image_count_);
        imgui_command_buffers_ =
            logical_device_.allocateCommandBuffers(command_buffer_info);
    }

    // Create framebuffers
    {
        imgui_frame_buffers_.resize(image_count_);
        for (uint32_t i = 0; i < image_count_; ++i) {
            vk::FramebufferCreateInfo info(
                vk::FramebufferCreateFlags(), imgui_render_pass_,
                swap_chain_image_views_[i], swap_chain_extent_.width,
                swap_chain_extent_.height, 1);
            imgui_frame_buffers_[i] = logical_device_.createFramebuffer(info);
        }
    }

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window_, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance_;
    init_info.PhysicalDevice = physical_device_;
    init_info.Device = logical_device_;
    init_info.QueueFamily =
        FindQueueFamilies(physical_device_).graphics_family.value().index;
    init_info.Queue = graphics_queue_;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_descriptor_pool_;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = image_count_;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, imgui_render_pass_);

    auto command_buffer = BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    EndSingleTimeCommands(command_buffer);
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE