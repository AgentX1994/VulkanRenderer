#include "application.h"

#include <algorithm>
#include <iostream>
#include <limits>

#include "utils.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
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

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
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
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPool();
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
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        logical_device_.destroySemaphore(render_finished_semaphore_[i]);
        logical_device_.destroySemaphore(image_available_semaphore_[i]);
        logical_device_.destroyFence(in_flight_fences_[i]);
    }
    logical_device_.destroyCommandPool(command_pool_);
    for (auto framebuffer : swap_chain_frame_buffers_) {
        logical_device_.destroyFramebuffer(framebuffer);
    }
    logical_device_.destroyPipeline(graphics_pipeline_);
    logical_device_.destroyPipelineLayout(pipeline_layout_);
    logical_device_.destroyRenderPass(render_pass_);
    for (auto image_view : swap_chain_image_views_) {
        logical_device_.destroyImageView(image_view);
    }
    logical_device_.destroySwapchainKHR(swapchain_);
    instance_.destroySurfaceKHR(surface_);
    logical_device_.destroy();
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        instance_.destroyDebugUtilsMessengerEXT(debug_messenger_);
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
    return indices.IsComplete() && extensions_supported && swapchain_adequate;
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

void Application::CreateImageViews()
{
    swap_chain_image_views_.resize(swap_chain_images_.size());

    for (size_t i = 0; i < swap_chain_images_.size(); ++i) {
        vk::ImageViewCreateInfo create_info(
            vk::ImageViewCreateFlags(),
            swap_chain_images_[i],
            vk::ImageViewType::e2D,
            swap_chain_image_format_,
            vk::ComponentSwizzle(),
            vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor,
                0,
                1,
                0,
                1));

        swap_chain_image_views_[i] = logical_device_.createImageView(create_info);
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

    vk::PipelineVertexInputStateCreateInfo vertex_input_info(
        vk::PipelineVertexInputStateCreateFlags(),
        {}, // Vertex Binding Descriptions
        {} // Vertex attribute descriptions
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
        vk::FrontFace::eClockwise,
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
        {}, // Set layouts
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

        command_buffers_[i].draw(3, 1, 0, 0);

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
    logical_device_.waitForFences(in_flight_fences_[current_frame_], VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Get next image
    auto result = logical_device_.acquireNextImageKHR(swapchain_, std::numeric_limits<uint64_t>::max(), image_available_semaphore_[current_frame_], {});
    uint32_t image_index;
    switch (result.result) {
    case vk::Result::eSuccess:
        image_index = result.value;
        break;
    case vk::Result::eSuboptimalKHR:
    case vk::Result::eNotReady:
    case vk::Result::eTimeout:
        throw std::runtime_error("Could not acquire next image!");
    }

    // Check if a previous frame is using this image
    // operator bool() is true if not VK_NULL_HANDLE
    if (images_in_flight_[image_index]) {
        logical_device_.waitForFences(images_in_flight_[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    images_in_flight_[image_index] = in_flight_fences_[current_frame_];

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

    auto present_result = present_queue_.presentKHR(present_info);

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE