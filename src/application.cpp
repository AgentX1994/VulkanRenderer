#include "application.h"

#ifdef WIN32
#else
#include <fontconfig/fontconfig.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
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

constexpr uint64_t MAX_FPS_DATA_COUNT = 10;
constexpr double FPS_GRAPH_UPDATE_TIME = 0.1;

const std::vector<std::string> MODEL_PATHS = {"models/viking_room.obj"};
const std::string TEXTURE_PATH = "textures/viking_room.png";

const std::map<const char*, vk::SampleCountFlagBits> SAMPLE_COUNT_MAP = {
    {"1 Sample", vk::SampleCountFlagBits::e1},
    {"2 Samples", vk::SampleCountFlagBits::e2},
    {"4 Samples", vk::SampleCountFlagBits::e4},
    {"8 Samples", vk::SampleCountFlagBits::e8},
    {"16 Samples", vk::SampleCountFlagBits::e16},
    {"32 Samples", vk::SampleCountFlagBits::e32},
    {"64 Samples", vk::SampleCountFlagBits::e64}};

const std::map<vk::SampleCountFlagBits, const char*> REVERSE_SAMPLE_COUNT_MAP =
    {{vk::SampleCountFlagBits::e1, "1 Sample"},
     {vk::SampleCountFlagBits::e2, "2 Samples"},
     {vk::SampleCountFlagBits::e4, "4 Samples"},
     {vk::SampleCountFlagBits::e8, "8 Samples"},
     {vk::SampleCountFlagBits::e16, "16 Samples"},
     {vk::SampleCountFlagBits::e32, "32 Samples"},
     {vk::SampleCountFlagBits::e64, "64 Samples"}};
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
    std::ofstream& log_file = *static_cast<std::ofstream*>(pUserData);
    log_file << pCallbackData->pMessage << std::endl;
    std::cout << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void PopulateDebugInfo(vk::DebugUtilsMessengerCreateInfoEXT& messenger_info,
                       void* ctxt = nullptr)
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
    messenger_info.setPUserData(ctxt);
}

static void check_vk_result(VkResult err)
{
    if (err == 0) return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0) abort();
}

struct GlfwCallbacks
{
    static void FramebufferResizeCallback(GLFWwindow* window, int width,
                                          int height)
    {
        auto app =
            reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->SetFramebufferResized();
    }

    static void WindowContentScaleCallback(GLFWwindow* window, float xscale,
                                           float yscale)
    {
        assert(xscale == yscale);
        auto app =
            reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->SetRenderScaling(xscale);
    }

    static void KeyCallback(GLFWwindow* window, int key, int scancode,
                            int action, int mods)
    {
        auto app =
            reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        Input::GlfwCallbacks::KeyCallback(&app->input_.value(), key, scancode,
                                          action, mods);
    }

    static void MouseCallback(GLFWwindow* window, double xpos, double ypos)
    {
        auto app =
            reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        Input::GlfwCallbacks::MouseCallback(&app->input_.value(), xpos, ypos);
    }
    static void MouseEnterCallback(GLFWwindow* window, int entered)
    {
        auto app =
            reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        Input::GlfwCallbacks::MouseEnterCallback(&app->input_.value(),
                                                 entered != 0);
    }
};

static void SetCaptureCursor(GLFWwindow* window, bool capture)
{
    if (capture) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else {
        if (glfwRawMouseMotionSupported()) {
            glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        // prevent jump when going back to normal mode
        GlfwCallbacks::MouseEnterCallback(window, true);
    }
}

void Application::Run()
{
    Init();
    MainLoop();
    Cleanup();
}

void Application::Init()
{
    InitWindow();
    InitVulkan();
    SetupImgui();
    LoadScene();
}

void Application::MainLoop()
{
    double previous_time = glfwGetTime();
    while (!glfwWindowShouldClose(window_)) {
        // Calls glfwPollEvents for us
        input_->Poll();
        if (input_->GetActionInputState(InputAction::Quit)) {
            break;
        }

        if (input_->GetActionInputState(InputAction::ToggleImgui)) {
            if (!imgui_toggle_pressed_last_frame_) {
                imgui_display_ ^= true;
                if (active_camera_ == controlled_camera_) {
                    SetCaptureCursor(window_, !imgui_display_);
                } else {
                    SetCaptureCursor(window_, false);
                }
            }
            imgui_toggle_pressed_last_frame_ = true;
        } else {
            imgui_toggle_pressed_last_frame_ = false;
        }

        // Get how much time the last frame took
        double current_time = glfwGetTime();
        double delta = current_time - previous_time;
        previous_time = current_time;

        // Update FPS counter
        current_frames_per_second_ = 1.0f / (float)delta;

        fps_timer_ += delta;
        if (fps_timer_ > FPS_GRAPH_UPDATE_TIME) {
            fps_timer_ -= FPS_GRAPH_UPDATE_TIME;
            frames_per_second_data_.push_back(current_frames_per_second_);
            if (frames_per_second_data_.size() > MAX_FPS_DATA_COUNT) {
                // This is inefficient but good enough for now
                frames_per_second_data_.erase(frames_per_second_data_.begin());
            }
        }

        // Update scene state
        Update(delta);

        // Render new scene state
        Render();
    }

    renderer_->GetDevice().waitIdle();
}

void Application::Update(double delta_time)
{
    UpdateRotatingCamera(delta_time);
    UpdateControlledCamera(delta_time);
}

void Application::Render()
{
    WaitForNextFrameFence();
    auto image_index_optional = GetNextImage();
    if (!image_index_optional.has_value()) {
        RecreateSwapChain();
        return;
    }
    uint32_t image_index = *image_index_optional;
    WaitForImageFenceAndSetNewFence(image_index);

    UpdateCameraUniformBuffer();

    DrawScene(frame_data_[current_frame_],
              renderer_->GetFramebuffers()[image_index],
              command_buffers_[image_index]);

    // For updating MSAA samples
    auto msaa_samples = renderer_->GetCurrentSampleCount();

    DrawGui(imgui_frame_buffers_[image_index],
            imgui_command_buffers_[image_index], msaa_samples);

    bool should_update_samples =
        msaa_samples != renderer_->GetCurrentSampleCount();

    std::vector<vk::CommandBuffer> command_buffers_to_submit = {
        command_buffers_[image_index], imgui_command_buffers_[image_index]};

    SubmitGraphicsCommands(command_buffers_to_submit);

    Present(image_index);

    if (should_update_samples) {
        renderer_->UpdateCurrentSampleCount(msaa_samples);
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

    renderer_->GetDevice().freeCommandBuffers(
        renderer_->GetGraphicsCommandPool(), command_buffers_);
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
        renderer_->GetInstance().destroyDebugUtilsMessengerEXT(
            debug_messenger_);
    }
    renderer_.reset();

    glfwDestroyWindow(window_);
    glfwTerminate();
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
    glfwSetFramebufferSizeCallback(window_,
                                   GlfwCallbacks::FramebufferResizeCallback);
    glfwSetWindowContentScaleCallback(
        window_, GlfwCallbacks::WindowContentScaleCallback);

    // Now that GLFW is initialized, create the input
    input_.emplace(window_);
    glfwSetKeyCallback(window_, GlfwCallbacks::KeyCallback);
    glfwSetCursorPosCallback(window_, GlfwCallbacks::MouseCallback);
    glfwSetCursorEnterCallback(window_, GlfwCallbacks::MouseEnterCallback);
}

void Application::SetFramebufferResized() { framebuffer_resized_ = true; }

void Application::SetRenderScaling(float scale)
{
    window_scaling_ = scale;

    // Rescale ImGui
    ResizeImGui();
}

void Application::InitVulkan()
{
    CreateRenderer();
    SetupDebugMessenger();
    CreateFrameData();
    CreateCameraDescriptorSets();
    CreateCommandBuffers();
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

void Application::SetupDebugMessenger()
{
    if constexpr (ENABLE_VALIDATION_LAYERS) {
        log_file_.open("validation_layer_errors.log");
        vk::DebugUtilsMessengerCreateInfoEXT messenger_info;
        PopulateDebugInfo(messenger_info, static_cast<void*>(&log_file_));
        debug_messenger_ =
            renderer_->GetInstance().createDebugUtilsMessengerEXT(
                messenger_info, nullptr);
    }
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

void Application::RecreateSwapChain()
{
    renderer_->RecreateSwapchain(window_);

    CleanupSwapChain();

    CreateCommandBuffers();

    ImGui_ImplVulkan_SetMinImageCount(
        renderer_->GetSwapchain().GetMinimumImageCount());
    CreateImGuiCommandBuffers();
    CreateImGuiFramebuffers();

    // Update camera aspect ratios
    auto extent = renderer_->GetSwapchain().GetExtent();
    float aspect_ratio = extent.width / (float)extent.height;
    for (auto& cam : cameras_) {
        cam.SetAspectRatio(aspect_ratio);
    }
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

void Application::UpdateCameraUniformBuffer()
{
    GpuCameraData camera = active_camera_->GetCameraData();

    auto& frame_data = frame_data_[current_frame_];

    void* data = renderer_->GetDevice().mapMemory(
        frame_data.camera_uniform_buffer->GetMemory(), 0, sizeof(camera));
    memcpy(data, &camera, sizeof(GpuCameraData));
    renderer_->GetDevice().unmapMemory(
        frame_data.camera_uniform_buffer->GetMemory());
}

void Application::LoadScene()
{
    for (const auto& path : MODEL_PATHS) {
        models_.emplace_back(renderer_.value(), path);
    }

    NonOwningPointer<SceneNode> root = scene_graph_.GetRoot();

    // The viking house model is set with up along the z axis,
    // so it needs to be rotated
    glm::quat viking_house_rotation =
        glm::angleAxis(glm::half_pi<float>(), glm::vec3(-1.0, 0.0, 0.0));
    {
        render_objects_.emplace_back(*renderer_);
        auto& obj1 = render_objects_.back();
        auto scene_node1 = root->CreateChildNode();
        scene_node1->SetTranslation(glm::vec3(-1.0, 0.0, 0.0));
        scene_node1->SetRotation(viking_house_rotation);
        obj1.SetModel(&models_.front());
        scene_node1->SetRenderObject(&obj1);
    }

    {
        render_objects_.emplace_back(*renderer_);
        auto& obj2 = render_objects_.back();
        auto scene_node2 = root->CreateChildNode();
        scene_node2->SetTranslation(glm::vec3(1.0, 0.0, 0.0));
        scene_node2->SetRotation(viking_house_rotation);
        obj2.SetModel(&models_.front());
        scene_node2->SetRenderObject(&obj2);
    }

    // Create camera nodes
    auto extent = renderer_->GetSwapchain().GetExtent();
    float aspect_ratio = extent.width / (float)extent.height;
    {
        rotating_camera_ = &cameras_[0];
        auto camera_node = root->CreateChildNode();
        camera_node->SetCamera(rotating_camera_);
        rotating_camera_->SetAspectRatio(aspect_ratio);
    }
    {
        controlled_camera_ = &cameras_[1];
        auto camera_node = root->CreateChildNode();
        camera_node->SetCamera(controlled_camera_);
        controlled_camera_->SetPosition({2.0, 2.0, 2.0});
        controlled_camera_->LookAt({0.0, 0.0, 0.0});
        controlled_camera_->SetAspectRatio(aspect_ratio);
    }
    active_camera_ = controlled_camera_;

    if (active_camera_ == controlled_camera_) {
        SetCaptureCursor(window_, !imgui_display_);
    }
}

void Application::UpdateRotatingCamera(double delta_time)
{
    auto pos = glm::vec3(
        3.0f * glm::cos(glm::radians(current_camera_rotation_degrees_)), 2.0f,
        3.0f * glm::sin(glm::radians(current_camera_rotation_degrees_)));
    rotating_camera_->SetPosition(pos);
    rotating_camera_->LookAt(glm::vec3(0.0f, 0.0f, 0.0f));
    // rotation_rate_ = RPM
    // RPS = RPM * 60
    current_camera_rotation_degrees_ = (float)delta_time * rotation_rate_ * 60 +
                                       current_camera_rotation_degrees_;
    current_camera_rotation_degrees_ =
        std::fmod(current_camera_rotation_degrees_, 360.0f);
}

void Application::UpdateControlledCamera(double delta_time)
{
    if (active_camera_ != controlled_camera_) {
        return;
    }

    // Movement
    float camera_speed = (float)delta_time * camera_movement_speed_;
    float roll_speed = (float)delta_time * camera_roll_speed_;
    if (input_->GetActionInputState(InputAction::Slow)) {
        camera_speed *= slowdown_factor_;
        roll_speed *= slowdown_factor_;
    }
    if (input_->GetActionInputState(InputAction::MoveForward)) {
        controlled_camera_->MoveForward(camera_speed);
    } else if (input_->GetActionInputState(InputAction::MoveBackward)) {
        controlled_camera_->MoveForward(-camera_speed);
    }

    if (input_->GetActionInputState(InputAction::MoveRight)) {
        controlled_camera_->MoveRight(camera_speed);
    } else if (input_->GetActionInputState(InputAction::MoveLeft)) {
        controlled_camera_->MoveRight(-camera_speed);
    }

    if (input_->GetActionInputState(InputAction::MoveUp)) {
        controlled_camera_->MoveUp(camera_speed);
    } else if (input_->GetActionInputState(InputAction::MoveDown)) {
        controlled_camera_->MoveUp(-camera_speed);
    }

    float roll_movement = 0.0f;
    if (input_->GetActionInputState(InputAction::RollRight)) {
        roll_movement = -roll_speed;
    } else if (input_->GetActionInputState(InputAction::RollLeft)) {
        roll_movement = roll_speed;
    }

    // Don't use mouse input if the gui is displayed
    if (imgui_display_) {
        return;
    }

    // Rotation
    auto mouse_movement = (float)delta_time * input_->GetMouseMovement();
    glm::vec3 rotation = glm::radians(
        glm::vec3(mouse_movement.y, -mouse_movement.x, roll_movement));
    if (rotation == glm::vec3(0.0f, 0.0f, 0.0f)) {
        return;
    }
    // glm::quat x_rotation = glm::angleAxis(glm::radians(-mouse_movement.x),
    //                                      glm::vec3(0.0f, 1.0f, 0.0f));
    // glm::quat y_rotation = glm::angleAxis(glm::radians(mouse_movement.y),
    //                                      glm::vec3(1.0f, 0.0f, 0.0f));
    // glm::quat z_rotation = glm::angleAxis(glm::radians(roll_movement),
    //                                      glm::vec3(0.0f, 0.0f, 1.0f));

    // x and y are independent, so we can just multiply them together to compose
    // them (I think...)
    // glm::quat rotation = x_rotation * y_rotation * z_rotation;
    controlled_camera_->Rotate(rotation);
}

void Application::WaitForNextFrameFence()
{
    // Wait until this fence has been finished
    auto wait_for_fence_result = renderer_->GetDevice().waitForFences(
        frame_data_[current_frame_].in_flight_fence, VK_TRUE,
        std::numeric_limits<uint64_t>::max());
    if (wait_for_fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("Could not wait for fence!");
    }
}

std::optional<uint32_t> Application::GetNextImage()
{
    // Get next image
    vk::ResultValue<uint32_t> result(vk::Result::eSuccess, 0);
    result = renderer_->GetSwapchain().GetNextImage(
        std::numeric_limits<uint64_t>::max(),
        frame_data_[current_frame_].image_available_semaphore, {});
    switch (result.result) {
        case vk::Result::eSuccess:
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eNotReady:
            return result.value;
            break;
        case vk::Result::eErrorOutOfDateKHR:
            return std::nullopt;
        case vk::Result::eTimeout:
            throw std::runtime_error("Could not acquire next image (timeout)!");
        default:
            throw std::runtime_error(
                "Could not acquire next image (unknown error)!");
    }
}

void Application::WaitForImageFenceAndSetNewFence(uint32_t image_index)
{
    // Check if a previous frame is using this image
    // operator bool() is true if not VK_NULL_HANDLE
    if (images_in_flight_[image_index]) {
        auto wait_result = renderer_->GetDevice().waitForFences(
            images_in_flight_[image_index], VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        if (wait_result != vk::Result::eSuccess) {
            throw std::runtime_error("Could not wait for image fence!");
        }
    }

    images_in_flight_[image_index] =
        frame_data_[current_frame_].in_flight_fence;
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
                material->GetGraphicsPipelineLayout(), 1,
                material->GetDescriptorSet(), {});
        }

        // bind object properties
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          material->GetGraphicsPipelineLayout(),
                                          2, obj.GetDescriptorSet(), {});

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

void Application::DrawGui(vk::Framebuffer& framebuffer,
                          vk::CommandBuffer& command_buffer,
                          vk::SampleCountFlagBits& msaa_samples)
{
    auto msaa_samples_str = REVERSE_SAMPLE_COUNT_MAP.at(msaa_samples);
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (imgui_display_) {
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

            if (ImGui::TreeNode("Camera")) {
                ImGui::Text("Camera Type");
                if (ImGui::RadioButton("Rotating",
                                       active_camera_ == rotating_camera_)) {
                    active_camera_ = rotating_camera_;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton("Controlled",
                                       active_camera_ == controlled_camera_)) {
                    active_camera_ = controlled_camera_;
                }

                if (ImGui::TreeNode("Properties")) {
                    if (active_camera_ == rotating_camera_) {
                        ImGui::DragFloat("Camera Rotation Rate",
                                         &rotation_rate_, 0.1f, -60.0f, 60.0f,
                                         "%.02f RPM", ImGuiSliderFlags_None);
                    } else if (active_camera_ == controlled_camera_) {
                        glm::vec3 pos =
                            controlled_camera_->GetNode()->GetTranslation();
                        ImGui::Text("Position: %.02f %.02f %.02f", pos.x, pos.y,
                                    pos.z);

                        glm::vec3 euler = controlled_camera_->GetAngles();
                        euler = glm::degrees(euler);
                        if (ImGui::DragFloat3("Rotation", &euler[0], 1.0f,
                                              -180.0f, 180.0f)) {
                            euler = glm::radians(euler);
                            controlled_camera_->SetAngles(euler);
                        }

                        ImGui::DragFloat("Camera Movement Speed",
                                         &camera_movement_speed_, 0.1f, 0.0f,
                                         500.0f);

                        auto mouse_sensitivity = input_->GetMouseSensitivity();
                        if (ImGui::DragFloat("Mouse Sensitivity",
                                             &mouse_sensitivity, 0.1f, 0.0f,
                                             100.0f)) {
                            input_->SetMouseSensitivity(mouse_sensitivity);
                        }

                        ImGui::DragFloat("Camera Roll Speed",
                                         &camera_roll_speed_, 0.1f, 0.0f,
                                         100.0f);

                        ImGui::DragFloat("Camera Slowdown Factor",
                                         &slowdown_factor_, 0.05f, 0.0f, 1.0f);
                    }
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            auto extent = renderer_->GetSwapchain().GetExtent();
            ImGui::Text("Framebuffer Size: %ux%u", extent.width, extent.height);

            auto max_msaa_sample_count = renderer_->GetMaxSampleCount();
            uint32_t max_msaa_sample_count_int =
                (uint32_t)max_msaa_sample_count;
            ImGui::Text("Max MSAA Sample Count: %u", max_msaa_sample_count_int);

            if (ImGui::BeginCombo("Current MSAA Sample Count",
                                  msaa_samples_str)) {
                for (auto& map_entry : SAMPLE_COUNT_MAP) {
                    if (map_entry.second > max_msaa_sample_count) {
                        break;
                    }
                    bool is_selected = map_entry.first == msaa_samples_str;
                    if (ImGui::Selectable(map_entry.first, is_selected)) {
                        msaa_samples_str = map_entry.first;
                        msaa_samples = map_entry.second;
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Text("%.02f FPS", current_frames_per_second_);
            ImGui::PlotLines("FPS Graph", frames_per_second_data_.data(),
                             (int)frames_per_second_data_.size(), 0, nullptr,
                             0.0f);
        }
        ImGui::End();
        if (!imgui_display_ && active_camera_ == controlled_camera_) {
            SetCaptureCursor(window_, true);
        }
    }
    ImGui::Render();
    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);
    vk::ClearValue clear_value;
    clear_value.color.setFloat32({{0.0f, 0.0f, 0.0f, 1.0f}});
    vk::RenderPassBeginInfo imgui_pass(
        imgui_render_pass_, framebuffer,
        {{0, 0}, renderer_->GetSwapchain().GetExtent()}, clear_value);
    command_buffer.beginRenderPass(imgui_pass, vk::SubpassContents::eInline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
    command_buffer.endRenderPass();
    command_buffer.end();
}

void Application::SubmitGraphicsCommands(
    std::vector<vk::CommandBuffer> command_buffers)
{
    vk::PipelineStageFlags wait_dest_stage_mask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submit_info(
        frame_data_[current_frame_].image_available_semaphore,
        wait_dest_stage_mask, command_buffers,
        frame_data_[current_frame_].render_finished_semaphore);

    renderer_->GetDevice().resetFences(
        frame_data_[current_frame_].in_flight_fence);

    renderer_->GetGraphicsQueue().submit(
        submit_info, frame_data_[current_frame_].in_flight_fence);
}

void Application::Present(uint32_t image_index)
{
    auto swapchain = renderer_->GetSwapchain().GetSwapchain();
    vk::PresentInfoKHR present_info(
        frame_data_[current_frame_].render_finished_semaphore, swapchain,
        image_index, {});

    vk::Result present_result = renderer_->Present(present_info);

    if (present_result == vk::Result::eSuboptimalKHR ||
        present_result == vk::Result::eErrorOutOfDateKHR ||
        framebuffer_resized_) {
        framebuffer_resized_ = false;
        RecreateSwapChain();
    } else if (present_result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to present swap chain image!");
    }
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

void Application::FindFontFile(std::string name)
{
#ifdef WIN32
    // Don't feel like figuring out how to find font file paths
    // on windows
    font_file_ = "C:\\Users\\John\\AppData\\Local\\Microsoft\\Windows\\Fonts";
#else
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
#endif
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

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE