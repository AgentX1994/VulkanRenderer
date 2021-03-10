#include "application.h"

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <unordered_map>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "camera.h"
#include "common.h"
#include "common_vulkan.h"
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
    LoadScene();
    CreateFrameData();
    CreateCameraDescriptorSets();
    CreateCommandBuffers();
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

    render_objects_.clear();

    renderer_->GetDevice().destroyCommandPool(imgui_command_pool_);
    renderer_->GetDevice().destroyRenderPass(imgui_render_pass_);
    renderer_->GetDevice().destroyDescriptorPool(imgui_descriptor_pool_);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame_data = frame_data_[i];
        frame_data.camera_uniform_buffer.reset();
        renderer_->GetDevice().destroySemaphore(
            frame_data.render_finished_semaphore);
        renderer_->GetDevice().destroySemaphore(
            frame_data.image_available_semaphore);
        renderer_->GetDevice().destroyFence(frame_data.in_flight_fence);
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
}

void Application::CreateFrameData()
{
    // Create these as empty (default) so that we can copy in_flight fences into
    // them
    images_in_flight_.resize(renderer_->GetSwapchain().GetActualImageCount());

    vk::SemaphoreCreateInfo semaphore_info;
    vk::FenceCreateInfo fence_info(
        vk::FenceCreateFlagBits::eSignaled);  // Create signaled so we don't get
                                              // stuck waiting for it

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame_data = frame_data_[i];

        frame_data.camera_uniform_buffer.emplace(
            *renderer_, sizeof(GpuCameraData),
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        // Descriptor sets are created in
        // Application::CreateCameraDescriptorSets

        frame_data.image_available_semaphore =
            renderer_->GetDevice().createSemaphore(semaphore_info);
        frame_data.render_finished_semaphore =
            renderer_->GetDevice().createSemaphore(semaphore_info);
        frame_data.in_flight_fence =
            renderer_->GetDevice().createFence(fence_info);
    }
}

void Application::DrawFrame()
{
    // Wait until this fence has been finished
    auto wait_for_fence_result = renderer_->GetDevice().waitForFences(
        frame_data_[current_frame_].in_flight_fence, VK_TRUE,
        std::numeric_limits<uint64_t>::max());
    if (wait_for_fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("Could not wait for fence!");
    }

    // Get next image
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    try {
        result = renderer_->GetSwapchain().GetNextImage(
            std::numeric_limits<uint64_t>::max(),
            frame_data_[current_frame_].image_available_semaphore, {});
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

    images_in_flight_[image_index] =
        frame_data_[current_frame_].in_flight_fence;

    UpdateCameraBuffer();

    DrawScene(frame_data_[current_frame_],
              renderer_->GetFramebuffers()[image_index],
              command_buffers_[image_index]);

    vk::PipelineStageFlags wait_dest_stage_mask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;

    // For updating MSAA samples
    const std::map<const char*, vk::SampleCountFlagBits>
        SAMPLE_COUNT_MAP = {{"1 Sample", vk::SampleCountFlagBits::e1},
                            {"2 Samples", vk::SampleCountFlagBits::e2},
                            {"4 Samples", vk::SampleCountFlagBits::e4},
                            {"8 Samples", vk::SampleCountFlagBits::e8},
                            {"16 Samples", vk::SampleCountFlagBits::e16},
                            {"32 Samples", vk::SampleCountFlagBits::e32},
                            {"64 Samples", vk::SampleCountFlagBits::e64}};
    const std::map<vk::SampleCountFlagBits, const char*>
        REVERSE_SAMPLE_COUNT_MAP = {
            {vk::SampleCountFlagBits::e1, "1 Sample"},
            {vk::SampleCountFlagBits::e2, "2 Samples"},
            {vk::SampleCountFlagBits::e4, "4 Samples"},
            {vk::SampleCountFlagBits::e8, "8 Samples"},
            {vk::SampleCountFlagBits::e16, "16 Samples"},
            {vk::SampleCountFlagBits::e32, "32 Samples"},
            {vk::SampleCountFlagBits::e64, "64 Samples"}};

    bool should_update_samples = false;
    auto msaa_samples = renderer_->GetCurrentSampleCount();
    auto msaa_samples_str = REVERSE_SAMPLE_COUNT_MAP.at(msaa_samples);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (ImGui::Begin("Stats", &imgui_display_)) {
        uint32_t vertex_count = 0;
        uint32_t tri_count = 0;
        for (auto& obj : render_objects_) {
            auto model = obj.GetModel();
            if (!model) {
                continue;
            }
            vertex_count += model->GetVertexCount();
            tri_count += model->GetTriangleCount();
        }

        ImGui::Text("%u vertices", vertex_count);
        ImGui::Text("%u triangles", tri_count);

        auto extent = renderer_->GetSwapchain().GetExtent();
        ImGui::Text("Framebuffer Size: %ux%u", extent.width, extent.height);

        auto max_msaa_sample_count = renderer_->GetMaxSampleCount();
        uint32_t max_msaa_sample_count_int = (uint32_t)max_msaa_sample_count;
        ImGui::Text("Max MSAA Sample Count: %u", max_msaa_sample_count_int);

        if (ImGui::BeginCombo("Current MSAA Sample Count", msaa_samples_str)) {
            for (auto& map_entry : SAMPLE_COUNT_MAP) {
                if (map_entry.second > max_msaa_sample_count) {
                    break;
                }
                bool is_selected = map_entry.first == msaa_samples_str;
                if (ImGui::Selectable(map_entry.first, is_selected)) {
                    msaa_samples_str = map_entry.first;
                    msaa_samples = map_entry.second;
                    should_update_samples = true;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
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

    vk::SubmitInfo submit_info(
        frame_data_[current_frame_].image_available_semaphore,
        wait_dest_stage_mask, command_buffers_to_submit,
        frame_data_[current_frame_].render_finished_semaphore);

    renderer_->GetDevice().resetFences(
        frame_data_[current_frame_].in_flight_fence);

    renderer_->GetGraphicsQueue().submit(
        submit_info, frame_data_[current_frame_].in_flight_fence);

    auto swapchain = renderer_->GetSwapchain().GetSwapchain();
    vk::PresentInfoKHR present_info(
        frame_data_[current_frame_].render_finished_semaphore, swapchain,
        image_index, {});

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

    if (should_update_samples) {
        renderer_->UpdateCurrentSampleCount(msaa_samples);
    }

    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::DrawScene(FrameData& frame_data, vk::Framebuffer& framebuffer,
                            vk::CommandBuffer& command_buffer)
{
    vk::CommandBufferBeginInfo begin_info(vk::CommandBufferUsageFlags(), {});

    command_buffer.begin(begin_info);

    std::array<vk::ClearValue, 2> clear_values;
    clear_values[0].color.setFloat32({{0.0f, 0.0f, 0.0f, 0.0f}});
    clear_values[1].depthStencil.setDepth(1.0f);
    clear_values[1].depthStencil.setStencil(0);

    vk::RenderPassBeginInfo render_pass_info(
        renderer_->GetRenderPass(), framebuffer,
        {{0, 0}, renderer_->GetSwapchain().GetExtent()}, clear_values);

    command_buffer.beginRenderPass(render_pass_info,
                                   vk::SubpassContents::eInline);

    NonOwningPointer<Material> last_material = nullptr;
    for (auto& obj : render_objects_) {
        auto model = obj.GetModel();
        auto material_name = model->GetMaterialName();
        auto material =
            renderer_->GetMaterialCache().GetMaterialByName(material_name);
        if (material == nullptr) {
            std::cerr << "Warning: material \"" << material_name
                      << "\" does not exist!\n";
            continue;
        }
        if (last_material != material) {
            command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                        material->GetGraphicsPipeline());
            // Bind camera
            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                material->GetGraphicsPipelineLayout(), 0,
                frame_data.camera_uniform_descriptor, {});

            // Bind texture
            command_buffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                material->GetGraphicsPipelineLayout(), 2,
                material->GetDescriptorSet(), {});
        }

        // bind object properties
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          material->GetGraphicsPipelineLayout(),
                                          1, obj.GetDescriptorSet(), {});

        for (auto& mesh : model->GetMeshes()) {
            command_buffer.bindVertexBuffers(0, mesh.GetVertexBuffer(), {0});
            command_buffer.bindIndexBuffer(mesh.GetIndexBuffer(), 0,
                                           vk::IndexType::eUint32);
            command_buffer.drawIndexed(mesh.GetTriangleCount() * 3u, 1, 0, 0,
                                       0);
        }
    }

    command_buffer.endRenderPass();
    command_buffer.end();
}

void Application::CleanupSwapChain()
{
    for (auto framebuffer : imgui_frame_buffers_) {
        renderer_->GetDevice().destroyFramebuffer(framebuffer);
    }
    renderer_->GetDevice().freeCommandBuffers(imgui_command_pool_,
                                              imgui_command_buffers_);

    uniform_buffers_.clear();

    renderer_->GetDevice().freeCommandBuffers(
        renderer_->GetGraphicsCommandPool(), command_buffers_);
}

void Application::RecreateSwapChain()
{
    renderer_->RecreateSwapchain(window_);

    CleanupSwapChain();

    CreateCommandBuffers();

    ImGui_ImplVulkan_SetMinImageCount(
        renderer_->GetSwapchain().GetMinimumImageCount());
    CreateImGuiCommandBuffers();
    CreateImGuiFramebuffers();
}

void Application::UpdateCameraBuffer()
{
    auto extent = renderer_->GetSwapchain().GetExtent();
    GpuCameraData camera{};
    auto pos = glm::vec3(
        3.0f * glm::cos(glm::radians(current_model_rotation_degrees_)),
        3.0f * glm::sin(glm::radians(current_model_rotation_degrees_)), 2.0f);
    camera.view = glm::lookAt(pos, glm::vec3(0.0f, 0.0f, 0.0f),
                              glm::vec3(0.0f, 0.0f, 1.0f));
    camera.proj = glm::perspective(
        glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
    // compensate for incorect y coordinate in clipping space (OpenGL has it
    // flipped compared to Vulkan)
    camera.proj[1][1] *= -1;
    camera.viewproj = camera.proj * camera.view;

    auto& frame_data = frame_data_[current_frame_];

    void* data = renderer_->GetDevice().mapMemory(
        frame_data.camera_uniform_buffer->GetMemory(), 0, sizeof(camera));
    memcpy(data, &camera, sizeof(GpuCameraData));
    renderer_->GetDevice().unmapMemory(
        frame_data.camera_uniform_buffer->GetMemory());
}

void Application::CreateCameraDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT, renderer_->GetCameraDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo alloc_info(renderer_->GetDescriptorPool(),
                                             layouts);

    auto camera_descriptor_sets =
        renderer_->GetDevice().allocateDescriptorSets(alloc_info);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame_data = frame_data_[i];
        frame_data.camera_uniform_descriptor = camera_descriptor_sets[i];

        // now write to the descriptor set
        vk::DescriptorBufferInfo buffer_info(
            frame_data.camera_uniform_buffer->GetBuffer(), 0,
            sizeof(GpuCameraData));

        std::array<vk::WriteDescriptorSet, 1> descriptor_writes = {
            {{frame_data.camera_uniform_descriptor,
              0,
              0,
              vk::DescriptorType::eUniformBuffer,
              {},
              buffer_info}}};

        renderer_->GetDevice().updateDescriptorSets(descriptor_writes, {});
    }
}

void Application::LoadScene()
{
    for (const auto& path : MODEL_PATHS) {
        models_.emplace_back(renderer_.value(), path);
    }

    NonOwningPointer<SceneNode> root = scene_graph_.GetRoot();

    {
        render_objects_.emplace_back(*renderer_);
        auto& obj1 = render_objects_.back();
        auto scene_node1 = root->CreateChildNode();
        scene_node1->SetTranslation(glm::vec3(-1.0, 0.0, 0.0));
        obj1.SetModel(&models_.front());
        scene_node1->SetRenderObject(&obj1);
    }

    {
        render_objects_.emplace_back(*renderer_);
        auto& obj2 = render_objects_.back();
        auto scene_node2 = root->CreateChildNode();
        scene_node2->SetTranslation(glm::vec3(1.0, 0.0, 0.0));
        obj2.SetModel(&models_.front());
        scene_node2->SetRenderObject(&obj2);
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
            vk::AttachmentDescriptionFlags(),
            renderer_->GetSwapchain().GetImageFormat().format,
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