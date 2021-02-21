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
#include "swapchain.h"
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

void Application::CreateCommandBuffers()
{
    vk::CommandBufferAllocateInfo alloc_info(
        renderer_->GetGraphicsCommandPool(), vk::CommandBufferLevel::ePrimary,
        renderer_->GetSwapchain().GetActualImageCount());

    command_buffers_ =
        renderer_->GetDevice().allocateCommandBuffers(alloc_info);

    auto& framebuffers = renderer_->GetFramebuffers();

    for (size_t i = 0; i < command_buffers_.size(); ++i) {
        vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlags(),
                                              {});

        command_buffers_[i].begin(begin_info);

        std::array<vk::ClearValue, 2> clear_values;
        clear_values[0].color.setFloat32({{0.0f, 0.0f, 0.0f, 0.0f}});
        clear_values[1].depthStencil.setDepth(1.0f);
        clear_values[1].depthStencil.setStencil(0);

        vk::RenderPassBeginInfo render_pass_info(
            renderer_->GetRenderPass(), framebuffers[i],
            {{0, 0}, renderer_->GetSwapchain().GetExtent()}, clear_values);

        command_buffers_[i].beginRenderPass(render_pass_info,
                                            vk::SubpassContents::eInline);


        for (auto& model : models_) {
            model.RecordDrawCommand(renderer_.value(), command_buffers_[i], descriptor_sets_[i]);
        }

        command_buffers_[i].endRenderPass();
        command_buffers_[i].end();
    }
}

void Application::CreateSyncObjects()
{
    // Create these as empty (default) so that we can copy in_flight fences into
    // them
    images_in_flight_.resize(renderer_->GetSwapchain().GetActualImageCount());

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
        result = renderer_->GetSwapchain().GetNextImage(
            std::numeric_limits<uint64_t>::max(),
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
        auto extent = renderer_->GetSwapchain().GetExtent();
        ImGui::Text("Framebuffer Size: %ux%u", extent.width, extent.height);
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
            {{0, 0}, renderer_->GetSwapchain().GetExtent()}, clear_value);
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

    auto swapchain = renderer_->GetSwapchain().GetSwapchain();
    vk::PresentInfoKHR present_info(render_finished_semaphore_[current_frame_],
                                    swapchain, image_index, {});

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

    uniform_buffers_.clear();

    renderer_->GetDevice().destroyDescriptorPool(descriptor_pool_);

    renderer_->GetDevice().freeCommandBuffers(
        renderer_->GetGraphicsCommandPool(), command_buffers_);
}

void Application::RecreateSwapChain()
{
    renderer_->RecreateSwapchain(window_);

    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();

    ImGui_ImplVulkan_SetMinImageCount(
        renderer_->GetSwapchain().GetMinimumImageCount());
    CreateImGuiCommandBuffers();
    CreateImGuiFramebuffers();
}

void Application::CreateUniformBuffers()
{
    vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

    for (size_t i = 0; i < renderer_->GetSwapchain().GetActualImageCount();
         ++i) {
        uniform_buffers_.emplace_back(
            renderer_.value(), buffer_size,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void Application::UpdateUniformBuffer(uint32_t index)
{
    auto extent = renderer_->GetSwapchain().GetExtent();
    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f),
                            glm::radians(current_model_rotation_degrees_),
                            glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
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
          static_cast<uint32_t>(
              renderer_->GetSwapchain().GetActualImageCount())},
         {vk::DescriptorType::eCombinedImageSampler,
          static_cast<uint32_t>(
              renderer_->GetSwapchain().GetActualImageCount())}}};

    vk::DescriptorPoolCreateInfo pool_info(
        vk::DescriptorPoolCreateFlags(),
        static_cast<uint32_t>(renderer_->GetSwapchain().GetActualImageCount()),
        pool_sizes);

    descriptor_pool_ = renderer_->GetDevice().createDescriptorPool(pool_info);
}

void Application::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        renderer_->GetSwapchain().GetActualImageCount(),
        renderer_->GetDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo alloc_info(descriptor_pool_, layouts);

    descriptor_sets_ =
        renderer_->GetDevice().allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < renderer_->GetSwapchain().GetActualImageCount();
         ++i) {
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

void Application::CreateTextureImage()
{
    texture_image_.emplace(renderer_.value(), TEXTURE_PATH);
}

void Application::CreateTextureSampler()
{
    auto properties = renderer_->GetPhysicalDevice().getProperties();
    vk::SamplerCreateInfo sampler_info(
        vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eRepeat, 0.0f,
        VK_TRUE, properties.limits.maxSamplerAnisotropy, VK_FALSE,
        vk::CompareOp::eAlways, 0.0f, static_cast<float>(texture_image_->GetMipLevels()),
        vk::BorderColor::eIntOpaqueBlack, VK_FALSE);

    texture_sampler_ = renderer_->GetDevice().createSampler(sampler_info);
}

void Application::LoadModel()
{
    for (const auto& path : MODEL_PATHS) {
        models_.push_back(Model(renderer_.value(), path));
    }
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
            vk::AttachmentDescriptionFlags(), renderer_->GetSwapchain().GetImageFormat().format,
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
    init_info.MinImageCount = renderer_->GetSwapchain().GetMinimumImageCount();
    init_info.ImageCount = renderer_->GetSwapchain().GetActualImageCount();
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, imgui_render_pass_);
    ResizeImGui();
}

void Application::CreateImGuiFramebuffers()
{
    auto image_count = renderer_->GetSwapchain().GetActualImageCount();
    imgui_frame_buffers_.resize(image_count);
    auto& image_views = renderer_->GetSwapchain().GetImageViews();
    auto extent = renderer_->GetSwapchain().GetExtent();

    for (uint32_t i = 0; i < image_count; ++i) {
        vk::FramebufferCreateInfo info(vk::FramebufferCreateFlags(),
                                       imgui_render_pass_, image_views[i],
                                       extent.width, extent.height, 1);
        imgui_frame_buffers_[i] =
            renderer_->GetDevice().createFramebuffer(info);
    }
}

void Application::CreateImGuiCommandBuffers()
{
    vk::CommandBufferAllocateInfo command_buffer_info(
        imgui_command_pool_, vk::CommandBufferLevel::ePrimary,
        renderer_->GetSwapchain().GetActualImageCount());
    imgui_command_buffers_ =
        renderer_->GetDevice().allocateCommandBuffers(command_buffer_info);
}

void Application::ResizeImGui()
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    io.Fonts->AddFontFromFileTTF(font_file_.c_str(),
                                 std::floor(window_scaling_ * 13.0f));

    auto command_buffer = renderer_->BeginSingleTimeCommands();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    renderer_->EndSingleTimeCommands(command_buffer);

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