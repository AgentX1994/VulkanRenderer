#include "application.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image.h"
#include "utils.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;

    static vk::VertexInputBindingDescription GetBindingDescription()
    {
        return vk::VertexInputBindingDescription(
            0,
            sizeof(Vertex),
            vk::VertexInputRate::eVertex);
    }

    static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        return {
            vk::VertexInputAttributeDescription(
                0,
                0,
                vk::Format::eR32G32Sfloat,
                offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(
                1,
                0,
                vk::Format::eR32G32B32Sfloat,
                offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(
                2,
                0,
                vk::Format::eR32G32Sfloat,
                offsetof(Vertex, tex_coord))
        };
    }
};

struct UniformBufferObject {
    alignas(4 * sizeof(float)) glm::mat4 model;
    alignas(4 * sizeof(float)) glm::mat4 view;
    alignas(4 * sizeof(float)) glm::mat4 proj;
};

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<Vertex> TRIANGLE = {
    { { 0.0f, -0.5f }, { 1.0f, 1.0f, 1.0f } },
    { { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } },
    { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } }
};

const std::vector<Vertex> SQUARE_VERTICES = {
    { { -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { 0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
    { { 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
    { { -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
};

const std::vector<uint16_t> SQUARE_INDICES = {
    0, 1, 2, 2, 3, 0
};

#ifndef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#endif

static VKAPI_ATTR uint32_t VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Application::Run()
{
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
}

void Application::SetFramebufferResized()
{
    framebuffer_resized_ = true;
}

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void Application::InitVulkan()
{
    CreateInstance();
    //SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPool();
    CreateTextureImage();
    CreateTextureImageView();
    CreateTextureSampler();
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
    CleanupSwapChain();

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
        //instance_.destroyDebugUtilsMessengerEXT(debug_messenger_);
    }
    instance_.destroy();

    glfwDestroyWindow(window_);
    glfwTerminate();
}

const bool CheckExtensions(const std::vector<vk::ExtensionProperties> supported_extensions, std::vector<const char*> required_extensions)
{
    for (const auto extension_name : required_extensions) {
        if (std::find_if(
                supported_extensions.begin(),
                supported_extensions.end(),
                [&extension_name](auto e) {
                    return strcmp(e.extensionName, extension_name) == 0;
                })
            == supported_extensions.end()) {
            return false;
        }
    }
    return true;
}

void PopulateDebugInfo(vk::DebugUtilsMessengerCreateInfoEXT& messenger_info)
{
    messenger_info.setMessageSeverity(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose);
    messenger_info.setMessageType(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance);
    messenger_info.setPfnUserCallback(DebugCallback);
    messenger_info.setPUserData(nullptr);
}

void Application::CreateInstance()
{
    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        if (!CheckValidationLayerSupport()) {
            throw std::runtime_error("Not all validation layers supported!");
        }
    }

    vk::ApplicationInfo info(
        "Hello Triangle",
        VK_MAKE_VERSION(1, 0, 0),
        "No Engine",
        VK_MAKE_VERSION(1, 0, 0),
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

    vk::InstanceCreateInfo create_info(
        vk::InstanceCreateFlags(),
        &info,
        layers,
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
        if (std::find_if(
                validation_layers.begin(),
                validation_layers.end(),
                [&layer](auto l) {
                    return strcmp(l.layerName, layer) == 0;
                })
            == validation_layers.end()) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> Application::GetRequiredExtensions()
{
    uint32_t glfw_required_extension_count;
    const char** glfw_required_extensions = glfwGetRequiredInstanceExtensions(&glfw_required_extension_count);

    std::vector<const char*> extensions(glfw_required_extensions, glfw_required_extensions + glfw_required_extension_count);

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
        debug_messenger_ = instance_.createDebugUtilsMessengerEXT(messenger_info, nullptr);
    }
}

QueueFamilyIndices Application::FindQueueFamilies(const vk::PhysicalDevice& device)
{
    QueueFamilyIndices indices;

    auto queue_families = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = i;
        }
        if (device.getSurfaceSupportKHR(i, surface_)) {
            indices.present_family = i;
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
        if (std::find_if(available_extensions.begin(), available_extensions.end(), [&extension](auto e) {
                return strcmp(e.extensionName, extension) == 0;
            })
            == available_extensions.end()) {
            return false;
        }
    }

    return true;
}

SwapChainSupportDetails Application::QuerySwapChainSupport(const vk::PhysicalDevice& device)
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
        swapchain_adequate = !details.formats.empty() && !details.present_modes.empty();
    }
    auto supported_features = device.getFeatures();
    return indices.IsComplete() && extensions_supported && swapchain_adequate && supported_features.samplerAnisotropy;
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

    vk::PhysicalDeviceProperties properties = physical_device_.getProperties();
    std::cout << "Found device:\n";
    std::cout << '\t' << properties.deviceName << '\n';
}

void Application::CreateLogicalDevice()
{
    auto indices = FindQueueFamilies(physical_device_);

    float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos = {
        { vk::DeviceQueueCreateFlags(), indices.graphics_family.value(), 1, &priority }, // Graphics queue
        { vk::DeviceQueueCreateFlags(), indices.present_family.value(), 1, &priority } // Present queue
    };

    vk::PhysicalDeviceFeatures device_features;
    device_features.samplerAnisotropy = VK_TRUE;

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

    graphics_queue_ = logical_device_.getQueue(indices.graphics_family.value(), 0);
    present_queue_ = logical_device_.getQueue(indices.present_family.value(), 0);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(logical_device_);
}

void Application::CreateSurface()
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = surface;
}

vk::SurfaceFormatKHR Application::ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats)
{
    for (const auto& format : available_formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }

    return available_formats[0];
}

vk::PresentModeKHR Application::ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& available_present_modes)
{
    for (const auto& present_mode : available_present_modes) {
        if (present_mode == vk::PresentModeKHR::eMailbox) {
            return present_mode;
        }
    }

    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Application::ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);

        vk::Extent2D actual_extent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actual_extent;
    }
}

void Application::CreateSwapChain()
{
    SwapChainSupportDetails details = QuerySwapChainSupport(physical_device_);

    auto surface_format = ChooseSwapSurfaceFormat(details.formats);
    auto present_mode = ChooseSwapPresentMode(details.present_modes);
    auto extent = ChooseSwapExtent(details.capabilities);

    uint32_t image_count = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0) {
        image_count = std::min(image_count, details.capabilities.maxImageCount);
    }

    auto indices = FindQueueFamilies(physical_device_);
    uint32_t queue_family_indices[] = {
        indices.graphics_family.value(),
        indices.present_family.value()
    };

    vk::SharingMode sharing_mode = vk::SharingMode::eExclusive;
    uint32_t queue_family_index_count = 0;
    const uint32_t* queue_family_indices_arg = nullptr;

    if (indices.graphics_family != indices.present_family) {
        sharing_mode = vk::SharingMode::eConcurrent;
        queue_family_index_count = 2;
        queue_family_indices_arg = queue_family_indices;
    }

    vk::SwapchainCreateInfoKHR swap_chain_info(
        vk::SwapchainCreateFlagsKHR(),
        surface_,
        image_count,
        surface_format.format,
        surface_format.colorSpace,
        extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        sharing_mode,
        queue_family_index_count,
        queue_family_indices_arg,
        details.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        present_mode,
        VK_TRUE);

    swapchain_ = logical_device_.createSwapchainKHR(swap_chain_info);
    swap_chain_images_ = logical_device_.getSwapchainImagesKHR(swapchain_);
    swap_chain_image_format_ = surface_format.format;
    swap_chain_extent_ = extent;
}

void Application::CreateRenderPass()
{
    vk::AttachmentDescription color_attachment(
        vk::AttachmentDescriptionFlags(),
        swap_chain_image_format_,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference color_attachment_ref(
        0,
        vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass(
        vk::SubpassDescriptionFlags(),
        vk::PipelineBindPoint::eGraphics,
        {},
        color_attachment_ref,
        {},
        {},
        {});

    vk::SubpassDependency dependency(
        VK_SUBPASS_EXTERNAL,
        0,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits { 0 },
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo render_pass_info(
        vk::RenderPassCreateFlags(),
        color_attachment,
        subpass,
        dependency);

    render_pass_ = logical_device_.createRenderPass(render_pass_info);
}

vk::ImageView Application::CreateImageView(vk::Image image, vk::Format format)
{
    vk::ImageViewCreateInfo view_info(
        vk::ImageViewCreateFlags(),
        image,
        vk::ImageViewType::e2D,
        format,
        vk::ComponentSwizzle(),
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1));

    return logical_device_.createImageView(view_info);
}

void Application::CreateImageViews()
{
    swap_chain_image_views_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
        swap_chain_image_views_[i] = CreateImageView(swap_chain_images_[i], swap_chain_image_format_);
    }
}

void Application::CreateGraphicsPipeline()
{
    auto vert_shader_bin = CompileShader("shaders/shader.vert", shaderc_glsl_vertex_shader);
    auto frag_shader_bin = CompileShader("shaders/shader.frag", shaderc_glsl_fragment_shader);

    auto vert_shader_module = CreateShaderModule(vert_shader_bin);
    auto frag_shader_module = CreateShaderModule(frag_shader_bin);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex,
        vert_shader_module,
        "main");
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment,
        frag_shader_module,
        "main");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = { vert_shader_stage_info, frag_shader_stage_info };

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input_info(
        vk::PipelineVertexInputStateCreateFlags(),
        bindingDescription, // Vertex Binding Descriptions
        attributeDescriptions // Vertex attribute descriptions
    );

    vk::PipelineInputAssemblyStateCreateInfo input_assembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleList,
        VK_FALSE);

    vk::Viewport viewport(
        0.0f,
        0.0f,
        (float)swap_chain_extent_.width,
        (float)swap_chain_extent_.height,
        0.0f,
        1.0f);
    vk::Rect2D scissor({ 0, 0 }, swap_chain_extent_);

    vk::PipelineViewportStateCreateInfo viewport_state(
        vk::PipelineViewportStateCreateFlags(),
        viewport,
        scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        vk::PipelineRasterizationStateCreateFlags(),
        VK_FALSE,
        VK_FALSE,
        vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise,
        VK_FALSE,
        0.0f,
        0.0f,
        0.0f,
        1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        vk::PipelineMultisampleStateCreateFlags(),
        vk::SampleCountFlagBits::e1,
        VK_FALSE,
        1.0f,
        nullptr,
        VK_FALSE,
        VK_FALSE);

    vk::PipelineColorBlendAttachmentState color_blend_attachment(
        VK_TRUE,
        vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo color_blending(
        vk::PipelineColorBlendStateCreateFlags(),
        VK_FALSE,
        vk::LogicOp::eCopy,
        color_blend_attachment,
        { 0.0f, 0.0f, 0.0f, 0.0f });

    std::vector<vk::DynamicState> dynamic_states = {
        //vk::DynamicState::eViewport,
        //vk::DynamicState::eLineWidth
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state(
        vk::PipelineDynamicStateCreateFlags(),
        dynamic_states);

    vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(),
        descriptor_set_layout_, // Set layouts
        {} // Push constant ranges
    );

    pipeline_layout_ = logical_device_.createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipeline_info(
        vk::PipelineCreateFlags(),
        shader_stages,
        &vertex_input_info,
        &input_assembly,
        {},
        &viewport_state,
        &rasterizer,
        &multisampling,
        {},
        &color_blending,
        &dynamic_state,
        pipeline_layout_,
        render_pass_,
        0,
        {},
        -1);

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

vk::ShaderModule Application::CreateShaderModule(const std::vector<uint32_t>& code)
{
    vk::ShaderModuleCreateInfo create_info(
        vk::ShaderModuleCreateFlagBits(),
        code);

    return logical_device_.createShaderModule(create_info);
}

void Application::CreateFramebuffers()
{
    swap_chain_frame_buffers_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_image_views_.size(); ++i) {
        vk::FramebufferCreateInfo framebuffer_info(
            vk::FramebufferCreateFlags(),
            render_pass_,
            swap_chain_image_views_[i],
            swap_chain_extent_.width,
            swap_chain_extent_.height,
            1);

        swap_chain_frame_buffers_[i] = logical_device_.createFramebuffer(framebuffer_info);
    }
}

void Application::CreateCommandPool()
{
    auto indices = FindQueueFamilies(physical_device_);

    vk::CommandPoolCreateInfo pool_info(
        vk::CommandPoolCreateFlags(),
        indices.graphics_family.value());

    command_pool_ = logical_device_.createCommandPool(pool_info);
}

std::pair<vk::Buffer, vk::DeviceMemory> Application::CreateBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo buffer_info(
        vk::BufferCreateFlags(),
        size,
        usage,
        vk::SharingMode::eExclusive);

    auto buffer = logical_device_.createBuffer(buffer_info);

    auto mem_reqs = logical_device_.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size,
        FindMemoryType(mem_reqs.memoryTypeBits, properties));

    auto memory = logical_device_.allocateMemory(alloc_info);
    logical_device_.bindBufferMemory(buffer, memory, 0);

    return { buffer, memory };
}

void Application::CreateVertexBuffer()
{
    vk::DeviceSize buffer_size = sizeof(SQUARE_VERTICES[0]) * SQUARE_VERTICES.size();

    auto [staging_buffer, staging_buffer_memory] = CreateBuffer(
        buffer_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, SQUARE_VERTICES.data(), (size_t)buffer_size);
    logical_device_.unmapMemory(staging_buffer_memory);

    std::tie(vertex_buffer_, vertex_buffer_memory_) = CreateBuffer(
        buffer_size,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    CopyBuffer(staging_buffer, vertex_buffer_, buffer_size);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

void Application::CreateIndexBuffer()
{
    vk::DeviceSize buffer_size = sizeof(SQUARE_INDICES[0]) * SQUARE_INDICES.size();

    auto [staging_buffer, staging_buffer_memory] = CreateBuffer(
        buffer_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = logical_device_.mapMemory(staging_buffer_memory, 0, buffer_size);
    memcpy(data, SQUARE_INDICES.data(), (size_t)buffer_size);
    logical_device_.unmapMemory(staging_buffer_memory);

    std::tie(index_buffer_, index_buffer_memory_) = CreateBuffer(
        buffer_size,
        vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    CopyBuffer(staging_buffer, index_buffer_, buffer_size);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

void Application::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo alloc_info(
        command_pool_,
        vk::CommandBufferLevel::ePrimary,
        swap_chain_frame_buffers_.size());

    command_buffers_ = logical_device_.allocateCommandBuffers(alloc_info);

    for (size_t i = 0; i < command_buffers_.size(); ++i) {
        vk::CommandBufferBeginInfo begin_info(
            vk::CommandBufferUsageFlags(),
            {});

        command_buffers_[i].begin(begin_info);

        vk::ClearColorValue clear_color(std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 1.0f }));
        vk::ClearValue clear_value(clear_color);

        vk::RenderPassBeginInfo render_pass_info(
            render_pass_,
            swap_chain_frame_buffers_[i],
            { { 0, 0 }, swap_chain_extent_ },
            clear_value);

        command_buffers_[i].beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

        command_buffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, graphics_pipeline_);

        command_buffers_[i].bindVertexBuffers(0, vertex_buffer_, { 0 });
        command_buffers_[i].bindIndexBuffer(index_buffer_, 0, vk::IndexType::eUint16);
        command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout_, 0, descriptor_sets_[i], {});
        command_buffers_[i].drawIndexed(
            static_cast<uint32_t>(SQUARE_INDICES.size()),
            1,
            0,
            0,
            0);

        command_buffers_[i].endRenderPass();
        command_buffers_[i].end();
    }
}

void Application::CreateSyncObjects()
{
    // Create these as empty (default) so that we can copy in_flight fences into them
    images_in_flight_.resize(swap_chain_images_.size());

    vk::SemaphoreCreateInfo semaphore_info;
    vk::FenceCreateInfo fence_info(vk::FenceCreateFlagBits::eSignaled); // Create signaled so we don't get stuck waiting for it

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        image_available_semaphore_[i] = logical_device_.createSemaphore(semaphore_info);
        render_finished_semaphore_[i] = logical_device_.createSemaphore(semaphore_info);
        in_flight_fences_[i] = logical_device_.createFence(fence_info);
    }
}

void Application::DrawFrame()
{
    // Wait until this fence has been finished
    auto wait_for_fence_result = logical_device_.waitForFences(in_flight_fences_[current_frame_], VK_TRUE, std::numeric_limits<uint64_t>::max());
    if (wait_for_fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("Could not wait for fence!");
    }

    // Get next image
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    try {
        result = logical_device_.acquireNextImageKHR(swapchain_, std::numeric_limits<uint64_t>::max(), image_available_semaphore_[current_frame_], {});
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
        auto wait_for_image_fence = logical_device_.waitForFences(images_in_flight_[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());

        if (wait_for_image_fence != vk::Result::eSuccess) {
            throw std::runtime_error("Could not wait for image fence!");
        }
    }

    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

    UpdateUniformBuffer(image_index);

    vk::PipelineStageFlags wait_dest_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info(
        image_available_semaphore_[current_frame_],
        wait_dest_stage_mask,
        command_buffers_[image_index],
        render_finished_semaphore_[current_frame_]);

    logical_device_.resetFences(in_flight_fences_[current_frame_]);

    graphics_queue_.submit(submit_info, in_flight_fences_[current_frame_]);

    vk::PresentInfoKHR present_info(
        render_finished_semaphore_[current_frame_],
        swapchain_,
        image_index,
        {});

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

    if (present_result == vk::Result::eSuboptimalKHR || present_result == vk::Result::eErrorOutOfDateKHR || framebuffer_resized_) {
        framebuffer_resized_ = false;
        RecreateSwapChain();
    } else if (present_result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::CleanupSwapChain()
{
    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
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
    CreateFramebuffers();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
}

uint32_t Application::FindMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties)
{
    auto mem_properties = physical_device_.getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void Application::CopyBuffer(vk::Buffer source, vk::Buffer destination, vk::DeviceSize size)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::BufferCopy copy_info(
        0,
        0,
        size);

    command_buffer.copyBuffer(source, destination, copy_info);

    EndSingleTimeCommands(command_buffer);
}

void Application::CreateDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding ubo_layout_binding(
        0,
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding sampler_layout_binding(
        1,
        vk::DescriptorType::eCombinedImageSampler,
        1,
        vk::ShaderStageFlagBits::eFragment);

std:
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };

    vk::DescriptorSetLayoutCreateInfo layout_info(
        vk::DescriptorSetLayoutCreateFlags(),
        bindings);

    descriptor_set_layout_ = logical_device_.createDescriptorSetLayout(layout_info);
}

void Application::CreateUniformBuffers()
{
    vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

    uniform_buffers_.resize(swap_chain_images_.size());
    uniform_buffers_memory_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
        std::tie(uniform_buffers_[i], uniform_buffers_memory_[i]) = CreateBuffer(buffer_size,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void Application::UpdateUniformBuffer(uint32_t index)
{
    static auto start_time = std::chrono::high_resolution_clock::now();

    auto current_time = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

    UniformBufferObject ubo {};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), swap_chain_extent_.width / (float)swap_chain_extent_.height, 0.1f, 10.0f);
    // compensate for incorect y coordinate in clipping space (OpenGL has it flipped compared to Vulkan)
    ubo.proj[1][1] *= -1;

    void* data = logical_device_.mapMemory(uniform_buffers_memory_[index], 0, sizeof(ubo));
    memcpy(data, &ubo, sizeof(ubo));
    logical_device_.unmapMemory(uniform_buffers_memory_[index]);
}

void Application::CreateDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> pool_sizes = { { { vk::DescriptorType::eUniformBuffer,
                                                               static_cast<uint32_t>(swap_chain_images_.size()) },
        { vk::DescriptorType::eCombinedImageSampler,
            static_cast<uint32_t>(swap_chain_images_.size()) } } };

    vk::DescriptorPoolCreateInfo pool_info(
        vk::DescriptorPoolCreateFlags(),
        static_cast<uint32_t>(swap_chain_images_.size()),
        pool_sizes);

    descriptor_pool_ = logical_device_.createDescriptorPool(pool_info);
}

void Application::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(swap_chain_images_.size(), descriptor_set_layout_);
    vk::DescriptorSetAllocateInfo alloc_info(
        descriptor_pool_,
        layouts);

    descriptor_sets_ = logical_device_.allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
        vk::DescriptorBufferInfo buffer_info(
            uniform_buffers_[i],
            0,
            sizeof(UniformBufferObject));

        vk::DescriptorImageInfo image_info(
            texture_sampler_,
            texture_image_view_,
            vk::ImageLayout::eShaderReadOnlyOptimal);

        std::array<vk::WriteDescriptorSet, 2> descriptor_writes = { { { descriptor_sets_[i],
                                                                          0,
                                                                          0,
                                                                          vk::DescriptorType::eUniformBuffer,
                                                                          {},
                                                                          buffer_info },
            { descriptor_sets_[i],
                1,
                0,
                vk::DescriptorType::eCombinedImageSampler,
                image_info } } };

        logical_device_.updateDescriptorSets(descriptor_writes, {});
    }
}

std::pair<vk::Image, vk::DeviceMemory> Application::CreateImage(
    uint32_t width,
    uint32_t height,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::DeviceSize image_size = width * height * 4;

    vk::ImageCreateInfo image_info(
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        { width, height, 1 },
        1,
        1,
        vk::SampleCountFlagBits::e1,
        tiling,
        usage,
        vk::SharingMode::eExclusive,
        {},
        vk::ImageLayout::eUndefined);

    vk::Image image = logical_device_.createImage(image_info);

    vk::MemoryRequirements mem_reqs = logical_device_.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size,
        FindMemoryType(mem_reqs.memoryTypeBits,
            properties));

    vk::DeviceMemory memory = logical_device_.allocateMemory(alloc_info);
    logical_device_.bindImageMemory(image, memory, 0);

    return { image, memory };
}

void Application::CreateTextureImage()
{
    int texture_width, texture_height, texture_channels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg", &texture_width, &texture_height, &texture_channels, STBI_rgb_alpha);
    vk::DeviceSize image_size = texture_width * texture_height * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image");
    }

    auto [staging_buffer, staging_buffer_memory] = CreateBuffer(
        image_size,
        vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void* data = logical_device_.mapMemory(staging_buffer_memory, 0, image_size);
    memcpy(data, pixels, static_cast<size_t>(image_size));
    logical_device_.unmapMemory(staging_buffer_memory);

    stbi_image_free(pixels);

    std::tie(texture_image_, texture_image_memory_) = CreateImage(
        texture_width,
        texture_height,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    TransitionImageLayout(
        texture_image_,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal);

    CopyBufferToImage(
        staging_buffer,
        texture_image_,
        texture_width,
        texture_height);

    TransitionImageLayout(
        texture_image_,
        vk::Format::eR8G8B8A8Srgb,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal);

    logical_device_.destroyBuffer(staging_buffer);
    logical_device_.freeMemory(staging_buffer_memory);
}

vk::CommandBuffer Application::BeginSingleTimeCommands()
{
    // TODO Transient CommandPool?
    vk::CommandBufferAllocateInfo alloc_info(
        command_pool_,
        vk::CommandBufferLevel::ePrimary,
        1);

    vk::CommandBuffer command_buffer = logical_device_.allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    command_buffer.begin(begin_info);
    return command_buffer;
}

void Application::EndSingleTimeCommands(vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    vk::SubmitInfo submit_info(
        {},
        {},
        command_buffer,
        {});

    graphics_queue_.submit(submit_info);
    graphics_queue_.waitIdle();

    logical_device_.freeCommandBuffers(command_pool_, command_buffer);
}

void Application::TransitionImageLayout(
    vk::Image image,
    vk::Format format,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::AccessFlags source_access_mask;
    vk::AccessFlags destination_access_mask;
    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        source_access_mask = vk::AccessFlags(0);
        destination_access_mask = vk::AccessFlagBits::eTransferWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        source_access_mask = vk::AccessFlagBits::eTransferWrite;
        destination_access_mask = vk::AccessFlagBits::eShaderRead;
        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    vk::ImageMemoryBarrier barrier(
        source_access_mask,
        destination_access_mask,
        old_layout,
        new_layout,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        image,
        vk::ImageSubresourceRange(
            vk::ImageAspectFlagBits::eColor,
            0,
            1,
            0,
            1));

    command_buffer.pipelineBarrier(
        source_stage,
        destination_stage,
        vk::DependencyFlags(),
        {},
        {},
        barrier);

    EndSingleTimeCommands(command_buffer);
}

void Application::CopyBufferToImage(
    vk::Buffer buffer,
    vk::Image image,
    uint32_t width,
    uint32_t height)
{
    auto command_buffer = BeginSingleTimeCommands();

    vk::BufferImageCopy region(
        0,
        0,
        0,
        vk::ImageSubresourceLayers(
            vk::ImageAspectFlagBits::eColor,
            0,
            0,
            1),
        { 0, 0, 0 },
        { width, height, 1 });

    command_buffer.copyBufferToImage(
        buffer,
        image,
        vk::ImageLayout::eTransferDstOptimal,
        region);

    EndSingleTimeCommands(command_buffer);
}

void Application::CreateTextureImageView()
{
    texture_image_view_ = CreateImageView(texture_image_, vk::Format::eR8G8B8A8Srgb);
}

void Application::CreateTextureSampler()
{
    auto properties = physical_device_.getProperties();
    vk::SamplerCreateInfo sampler_info(
        vk::SamplerCreateFlags(),
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eNearest,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.0f,
        VK_TRUE,
        properties.limits.maxSamplerAnisotropy,
        VK_FALSE,
        vk::CompareOp::eAlways,
        0.0f,
        0.0f,
        vk::BorderColor::eIntOpaqueBlack,
        VK_FALSE);

    texture_sampler_ = logical_device_.createSampler(sampler_info);
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE