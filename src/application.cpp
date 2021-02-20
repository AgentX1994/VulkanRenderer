#include "application.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <fontconfig/fontconfig.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "stb_image.h"
#include "tiny_obj_loader.h"
#include "utils.h"

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<std::string> MODEL_PATHS = {"models/viking_room.obj"};
const std::string TEXTURE_PATH = "textures/viking_room.png";

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

static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->SetFramebufferResized();
}

static void WindowContentScaleCallback(GLFWwindow* window, float xscale,
                                       float yscale)
{
    assert(xscale == yscale);
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->SetRenderScaling(xscale);
}

void Application::SetFramebufferResized() { framebuffer_resized_ = true; }
void Application::SetRenderScaling(float scale)
{
    window_scaling_ = scale;

    // Rescale ImGui
    ResizeImGui();
}

void Application::InitWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    window_ =
        glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    float xscale, yscale;
    glfwGetWindowContentScale(window_, &xscale, &yscale);
    assert(xscale == yscale);  // we are assuming this
    window_scaling_ = xscale;
    glfwSetFramebufferSizeCallback(window_, FramebufferResizeCallback);
}

void Application::InitVulkan()
{
    CreateRenderer();
    // SetupDebugMessenger();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateColorResources();
    CreateDepthResources();
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateTextureImage();
    CreateTextureSampler();
    LoadModel();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
}

void Application::MainLoop()
{
    double previous_time = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        double current_time = glfwGetTime();
        double delta = current_time - previous_time;
        frames_per_second_ = 1.0f / delta;
        Update(delta);
        DrawFrame();
        previous_time = current_time;
    }

    renderer_->GetDevice().waitIdle();
}

void Application::Cleanup()
{
    models_.clear();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupSwapChain();

    renderer_->GetDevice().destroyCommandPool(imgui_command_pool_);
    renderer_->GetDevice().destroyRenderPass(imgui_render_pass_);
    renderer_->GetDevice().destroyDescriptorPool(imgui_descriptor_pool_);

    texture_image_.reset();
    renderer_->GetDevice().destroySampler(texture_sampler_);

    renderer_->GetDevice().destroyDescriptorSetLayout(descriptor_set_layout_);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        renderer_->GetDevice().destroySemaphore(render_finished_semaphore_[i]);
        renderer_->GetDevice().destroySemaphore(image_available_semaphore_[i]);
        renderer_->GetDevice().destroyFence(in_flight_fences_[i]);
    }
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        // instance_.destroyDebugUtilsMessengerEXT(debug_messenger_);
    }
    renderer_.reset();

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

void Application::CreateRenderer()
{
    std::vector<const char*> layers;
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        layers = VALIDATION_LAYERS;
    }
    renderer_.emplace("Vulkan Renderer", window_, GetRequiredExtensions(),
                      DEVICE_EXTENSIONS, layers);
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
            renderer_->GetInstance().createDebugUtilsMessengerEXT(
                messenger_info, nullptr);
    }
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
    auto details =
        renderer_->QuerySwapChainSupport(renderer_->GetPhysicalDevice());

    auto surface_format = ChooseSwapSurfaceFormat(details.formats);
    auto present_mode = ChooseSwapPresentMode(details.present_modes);
    auto extent = ChooseSwapExtent(details.capabilities);

    min_image_count_ = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0) {
        min_image_count_ =
            std::min(min_image_count_, details.capabilities.maxImageCount);
    }

    auto indices = renderer_->GetQueueFamilies();
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
        vk::SwapchainCreateFlagsKHR(), renderer_->GetSurface(),
        min_image_count_, surface_format.format, surface_format.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment, sharing_mode,
        queue_family_index_count, queue_family_indices_arg,
        details.capabilities.currentTransform,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, present_mode, VK_TRUE);

    swapchain_ = renderer_->GetDevice().createSwapchainKHR(swap_chain_info);
    swap_chain_images_ =
        renderer_->GetDevice().getSwapchainImagesKHR(swapchain_);
    image_count_ = swap_chain_images_.size();
    swap_chain_image_format_ = surface_format.format;
    swap_chain_extent_ = extent;
}

void Application::CreateRenderPass()
{
    vk::AttachmentDescription color_attachment(
        vk::AttachmentDescriptionFlags(), swap_chain_image_format_,
        renderer_->GetMaxSampleCount(), vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depth_attachment(
        vk::AttachmentDescriptionFlags(), FindDepthFormat(),
        renderer_->GetMaxSampleCount(), vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
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

    render_pass_ = renderer_->GetDevice().createRenderPass(render_pass_info);
}

vk::ImageView Application::CreateImageView(vk::Image image, vk::Format format,
                                           vk::ImageAspectFlags aspect_flags,
                                           uint32_t mip_levels)
{
    vk::ImageViewCreateInfo view_info(
        vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, format,
        vk::ComponentSwizzle(),
        vk::ImageSubresourceRange(aspect_flags, 0, mip_levels, 0, 1));

    return renderer_->GetDevice().createImageView(view_info);
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
        vk::PipelineMultisampleStateCreateFlags(),
        renderer_->GetMaxSampleCount(), VK_TRUE, 0.2f, nullptr, VK_FALSE,
        VK_FALSE);

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
        renderer_->GetDevice().createPipelineLayout(pipeline_layout_info);

    vk::PipelineDepthStencilStateCreateInfo depth_stencil(
        vk::PipelineDepthStencilStateCreateFlags(), VK_TRUE, VK_TRUE,
        vk::CompareOp::eLess, VK_FALSE, VK_FALSE, {}, {});

    vk::GraphicsPipelineCreateInfo pipeline_info(
        vk::PipelineCreateFlags(), shader_stages, &vertex_input_info,
        &input_assembly, {}, &viewport_state, &rasterizer, &multisampling,
        &depth_stencil, &color_blending, &dynamic_state, pipeline_layout_,
        render_pass_, 0, {}, -1);

    auto result =
        renderer_->GetDevice().createGraphicsPipeline({}, pipeline_info);
    switch (result.result) {
        case vk::Result::eSuccess:
            graphics_pipeline_ = result.value;
            break;
        case vk::Result::ePipelineCompileRequiredEXT:
            throw std::runtime_error("Pipeline compile required (WTF?)");
    }

    renderer_->GetDevice().destroyShaderModule(frag_shader_module);
    renderer_->GetDevice().destroyShaderModule(vert_shader_module);
}

vk::ShaderModule Application::CreateShaderModule(
    const std::vector<uint32_t>& code)
{
    vk::ShaderModuleCreateInfo create_info(vk::ShaderModuleCreateFlagBits(),
                                           code);

    return renderer_->GetDevice().createShaderModule(create_info);
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
            renderer_->GetDevice().createFramebuffer(framebuffer_info);
    }
}

void Application::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo alloc_info(
        renderer_->GetGraphicsCommandPool(), vk::CommandBufferLevel::ePrimary,
        swap_chain_frame_buffers_.size());

    command_buffers_ =
        renderer_->GetDevice().allocateCommandBuffers(alloc_info);

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

        command_buffers_[i].bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                               pipeline_layout_, 0,
                                               descriptor_sets_[i], {});

        for (auto& model : models_) {
            model.RecordDrawCommand(command_buffers_[i]);
        }

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
            renderer_->GetDevice().createSemaphore(semaphore_info);
        render_finished_semaphore_[i] =
            renderer_->GetDevice().createSemaphore(semaphore_info);
        in_flight_fences_[i] = renderer_->GetDevice().createFence(fence_info);
    }
}

void Application::DrawFrame()
{
    // Wait until this fence has been finished
    auto wait_for_fence_result = renderer_->GetDevice().waitForFences(
        in_flight_fences_[current_frame_], VK_TRUE,
        std::numeric_limits<uint64_t>::max());
    if (wait_for_fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("Could not wait for fence!");
    }

    // Get next image
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    try {
        result = renderer_->GetDevice().acquireNextImageKHR(
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
        auto wait_for_image_fence = renderer_->GetDevice().waitForFences(
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
    if (ImGui::Begin("Stats", &imgui_display_)) {
        uint32_t vertex_count = 0;
        uint32_t tri_count = 0;
        for (auto& model : models_) {
            vertex_count += model.GetVertexCount();
            tri_count += model.GetTriangleCount();
        }
        ImGui::Text("%u vertices", vertex_count);
        ImGui::Text("%u triangles", tri_count);
        ImGui::Text("Framebuffer Size: %ux%u", swap_chain_extent_.width,
                    swap_chain_extent_.height);
        uint32_t msaa_sample_count = 1;
        switch (renderer_->GetMaxSampleCount()) {
            case vk::SampleCountFlagBits::e64:
                msaa_sample_count = 64;
                break;
            case vk::SampleCountFlagBits::e32:
                msaa_sample_count = 32;
                break;
            case vk::SampleCountFlagBits::e16:
                msaa_sample_count = 16;
                break;
            case vk::SampleCountFlagBits::e8:
                msaa_sample_count = 8;
                break;
            case vk::SampleCountFlagBits::e4:
                msaa_sample_count = 4;
                break;
            case vk::SampleCountFlagBits::e2:
                msaa_sample_count = 2;
                break;
            case vk::SampleCountFlagBits::e1:
                msaa_sample_count = 1;
                break;
            default:
                msaa_sample_count = 1;
                break;
        }
        ImGui::Text("MSAA Sample Count: %u", msaa_sample_count);
        ImGui::Text("%.02f FPS", frames_per_second_);
        ImGui::DragFloat("Rotation Rate", &rotation_rate_, 0.1f, -60.0f, 60.0f,
                         "%.02f RPM", ImGuiSliderFlags_None);
    }
    ImGui::End();
    ImGui::Render();
    {
        vk::CommandBufferBeginInfo begin_info(
            vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
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

    renderer_->GetDevice().resetFences(in_flight_fences_[current_frame_]);

    renderer_->GetGraphicsQueue().submit(submit_info,
                                         in_flight_fences_[current_frame_]);

    vk::PresentInfoKHR present_info(render_finished_semaphore_[current_frame_],
                                    swapchain_, image_index, {});

    vk::Result present_result = vk::Result::eSuccess;
    try {
        present_result = renderer_->GetPresentQueue().presentKHR(present_info);
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
        renderer_->GetDevice().destroyFramebuffer(framebuffer);
    }
    renderer_->GetDevice().freeCommandBuffers(imgui_command_pool_,
                                              imgui_command_buffers_);

    renderer_->GetDevice().destroyImageView(color_image_view_);
    renderer_->GetDevice().destroyImage(color_image_);
    renderer_->GetDevice().freeMemory(color_image_memory_);

    renderer_->GetDevice().destroyImageView(depth_image_view_);
    renderer_->GetDevice().destroyImage(depth_image_);
    renderer_->GetDevice().freeMemory(depth_image_memory_);

    uniform_buffers_.clear();

    renderer_->GetDevice().destroyDescriptorPool(descriptor_pool_);

    for (auto framebuffer : swap_chain_frame_buffers_) {
        renderer_->GetDevice().destroyFramebuffer(framebuffer);
    }
    renderer_->GetDevice().freeCommandBuffers(
        renderer_->GetGraphicsCommandPool(), command_buffers_);

    renderer_->GetDevice().destroyPipeline(graphics_pipeline_);
    renderer_->GetDevice().destroyPipelineLayout(pipeline_layout_);
    renderer_->GetDevice().destroyRenderPass(render_pass_);
    for (auto image_view : swap_chain_image_views_) {
        renderer_->GetDevice().destroyImageView(image_view);
    }
    renderer_->GetDevice().destroySwapchainKHR(swapchain_);
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

    renderer_->GetDevice().waitIdle();

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
    CreateImGuiCommandBuffers();
    CreateImGuiFramebuffers();
}

uint32_t Application::FindMemoryType(uint32_t type_filter,
                                     vk::MemoryPropertyFlags properties)
{
    auto mem_properties = renderer_->GetPhysicalDevice().getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
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
        renderer_->GetDevice().createDescriptorSetLayout(layout_info);
}

void Application::CreateUniformBuffers()
{
    vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

    for (size_t i = 0; i < image_count_; ++i) {
        uniform_buffers_.emplace_back(renderer_.value(), buffer_size, 
                           vk::BufferUsageFlagBits::eUniformBuffer,
                           vk::MemoryPropertyFlagBits::eHostVisible |
                               vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void Application::UpdateUniformBuffer(uint32_t index)
{
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f),
                            glm::radians(current_model_rotation_degrees_),
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

    void* data = renderer_->GetDevice().mapMemory(
        uniform_buffers_[index].GetMemory(), 0, sizeof(ubo));
    memcpy(data, &ubo, sizeof(ubo));
    renderer_->GetDevice().unmapMemory(uniform_buffers_[index].GetMemory());
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

    descriptor_pool_ = renderer_->GetDevice().createDescriptorPool(pool_info);
}

void Application::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(image_count_,
                                                 descriptor_set_layout_);
    vk::DescriptorSetAllocateInfo alloc_info(descriptor_pool_, layouts);

    descriptor_sets_ =
        renderer_->GetDevice().allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < image_count_; ++i) {
        vk::DescriptorBufferInfo buffer_info(uniform_buffers_[i].GetBuffer(), 0,
                                             sizeof(UniformBufferObject));

        vk::DescriptorImageInfo image_info(
            texture_sampler_, texture_image_->GetImageView(),
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

        renderer_->GetDevice().updateDescriptorSets(descriptor_writes, {});
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

    vk::Image image = renderer_->GetDevice().createImage(image_info);

    vk::MemoryRequirements mem_reqs =
        renderer_->GetDevice().getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size, FindMemoryType(mem_reqs.memoryTypeBits, properties));

    vk::DeviceMemory memory = renderer_->GetDevice().allocateMemory(alloc_info);
    renderer_->GetDevice().bindImageMemory(image, memory, 0);

    return {image, memory};
}

void Application::CreateTextureImage()
{
    texture_image_.emplace(renderer_.value(), TEXTURE_PATH);
}

vk::CommandBuffer Application::BeginSingleTimeCommands()
{
    // TODO Transient CommandPool?
    vk::CommandBufferAllocateInfo alloc_info(
        renderer_->GetTransientCommandPool(), vk::CommandBufferLevel::ePrimary,
        1);

    vk::CommandBuffer command_buffer =
        renderer_->GetDevice().allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    command_buffer.begin(begin_info);
    return command_buffer;
}

void Application::EndSingleTimeCommands(vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    vk::SubmitInfo submit_info({}, {}, command_buffer, {});

    renderer_->GetTransferQueue().submit(submit_info);
    renderer_->GetTransferQueue().waitIdle();

    renderer_->GetDevice().freeCommandBuffers(
        renderer_->GetTransientCommandPool(), command_buffer);
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

void Application::CreateTextureSampler()
{
    auto properties = renderer_->GetPhysicalDevice().getProperties();
    vk::SamplerCreateInfo sampler_info(
        vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f,
        VK_TRUE, properties.limits.maxSamplerAnisotropy, VK_FALSE,
        vk::CompareOp::eAlways, 0.0f, static_cast<float>(mip_levels_),
        vk::BorderColor::eIntOpaqueBlack, VK_FALSE);

    texture_sampler_ = renderer_->GetDevice().createSampler(sampler_info);
}

vk::Format Application::FindSupportedFormat(
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features)
{
    for (auto format : candidates) {
        auto props = renderer_->GetPhysicalDevice().getFormatProperties(format);
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

    std::tie(depth_image_, depth_image_memory_) = CreateImage(
        swap_chain_extent_.width, swap_chain_extent_.height, 1,
        renderer_->GetMaxSampleCount(), depth_format, vk::ImageTiling::eOptimal,
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
    for (const auto& path : MODEL_PATHS) {
        models_.push_back(Model(renderer_.value(), path));
    }
}

void Application::GenerateMipMaps(vk::Image image, vk::Format format,
                                  int32_t texture_width, int32_t texture_height,
                                  uint32_t mip_levels)
{
    auto format_properties =
        renderer_->GetPhysicalDevice().getFormatProperties(format);
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

void Application::CreateColorResources()
{
    vk::Format color_format = swap_chain_image_format_;

    std::tie(color_image_, color_image_memory_) = CreateImage(
        swap_chain_extent_.width, swap_chain_extent_.height, 1,
        renderer_->GetMaxSampleCount(), color_format, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eTransientAttachment |
            vk::ImageUsageFlagBits::eColorAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    color_image_view_ = CreateImageView(color_image_, color_format,
                                        vk::ImageAspectFlagBits::eColor, 1);
}

void Application::FindFontFile(std::string name)
{
    FcConfig* config = FcInitLoadConfigAndFonts();

    // configure the search pattern,
    // assume "name" is a std::string with the desired font name in it
    FcPattern* pat = FcNameParse((const FcChar8*)(name.c_str()));
    FcConfigSubstitute(config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    // find the font
    FcResult res;
    FcPattern* font = FcFontMatch(config, pat, &res);
    if (font) {
        FcChar8* file = NULL;
        if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
            // save the file to another std::string
            font_file_ = (char*)file;
        }
        FcPatternDestroy(font);
    }

    FcPatternDestroy(pat);
    FcConfigDestroy(config);
}

static void check_vk_result(VkResult err)
{
    if (err == 0) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

void Application::SetupImgui()
{
    // Find the font first
    FindFontFile("Fira Code");
    assert(font_file_.size() > 0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

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
            renderer_->GetDevice().createDescriptorPool(pool_info);
    }

    // Create imgui render pass
    {
        vk::AttachmentDescription attachment(
            vk::AttachmentDescriptionFlags(), swap_chain_image_format_,
            vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eLoad,
            vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eColorAttachmentOptimal,
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

        imgui_render_pass_ =
            renderer_->GetDevice().createRenderPass(render_pass_info);
    }

    // create command pool and buffers
    {
        vk::CommandPoolCreateInfo command_pool_info(
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            renderer_->GetQueueFamilies().graphics_family.value().index);
        imgui_command_pool_ =
            renderer_->GetDevice().createCommandPool(command_pool_info);

        CreateImGuiCommandBuffers();
    }

    // Create framebuffers
    CreateImGuiFramebuffers();

    ImGui_ImplGlfw_InitForVulkan(window_, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = renderer_->GetInstance();
    init_info.PhysicalDevice = renderer_->GetPhysicalDevice();
    init_info.Device = renderer_->GetDevice();
    init_info.QueueFamily =
        renderer_->GetQueueFamilies().graphics_family.value().index;
    init_info.Queue = renderer_->GetGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_descriptor_pool_;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = min_image_count_;
    init_info.ImageCount = image_count_;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, imgui_render_pass_);
    ResizeImGui();
}

void Application::CreateImGuiFramebuffers()
{
    imgui_frame_buffers_.resize(image_count_);
    for (uint32_t i = 0; i < image_count_; ++i) {
        vk::FramebufferCreateInfo info(
            vk::FramebufferCreateFlags(), imgui_render_pass_,
            swap_chain_image_views_[i], swap_chain_extent_.width,
            swap_chain_extent_.height, 1);
        imgui_frame_buffers_[i] =
            renderer_->GetDevice().createFramebuffer(info);
    }
}

void Application::CreateImGuiCommandBuffers()
{
    vk::CommandBufferAllocateInfo command_buffer_info(
        imgui_command_pool_, vk::CommandBufferLevel::ePrimary, image_count_);
    imgui_command_buffers_ =
        renderer_->GetDevice().allocateCommandBuffers(command_buffer_info);
}

void Application::ResizeImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF(font_file_.c_str(),
                                 std::floor(window_scaling_ * 13.0f));

    auto command_buffer = BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    EndSingleTimeCommands(command_buffer);

    ImGui::StyleColorsDark(&imgui_style_);
    imgui_style_.ScaleAllSizes(window_scaling_);
}

void Application::Update(float delta_time)
{
    // rotation_rate_ = RPM
    // RPS = RPM * 60
    current_model_rotation_degrees_ =
        delta_time * rotation_rate_ * 60 + current_model_rotation_degrees_;
    current_model_rotation_degrees_ =
        std::fmod(current_model_rotation_degrees_, 360.0f);
}

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE