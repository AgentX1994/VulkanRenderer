#include "renderer_state.h"

#include <iostream>
#include <unordered_map>

#include "swapchain.h"
#include "texture_cache.h"

const std::string ENGINE_NAME = "VulkanRenderer";

RendererState::RendererState(
    const std::string name, GLFWwindow* window,
    const std::vector<const char*>& required_instance_extensions,
    const std::vector<const char*>& required_device_extensions,
    const std::vector<const char*>& layers)
{
    instance_ = CreateInstance(name, required_instance_extensions, layers);
    surface_ = CreateSurface(window);

    physical_device_ = CreatePhysicalDevice(required_device_extensions);
    msaa_samples_ = GetMaxUsableSampleCount();

    queue_families_ = FindQueueFamilies(physical_device_);

    std::tie(device_, graphics_queue_, present_queue_, transfer_queue_) =
        CreateDeviceAndQueues(required_device_extensions, layers);

    graphics_command_pool_ =
        CreateCommandPool(queue_families_.graphics_family->index);

    transient_command_pool_ =
        CreateCommandPool(queue_families_.transfer_family->index);

    swapchain_.emplace(*this, window);
    CreateColorResources();
    CreateDepthResources();

    render_pass_ = CreateRenderPass();
    descriptor_set_layout_ = CreateDescriptorSetLayout();
}

RendererState::~RendererState()
{
    device_.waitIdle();

    texture_cache_.Clear();
    material_cache_.Clear();

    device_.destroyImageView(color_image_view_);
    color_image_.reset();
    device_.destroyImageView(depth_image_view_);
    depth_image_.reset();

    device_.destroyRenderPass(render_pass_);
    device_.destroyDescriptorSetLayout(descriptor_set_layout_);

    swapchain_.reset();
    device_.destroyCommandPool(transient_command_pool_);
    device_.destroyCommandPool(graphics_command_pool_);
    device_.destroy();
    instance_.destroySurfaceKHR(surface_);
    instance_.destroy();
}

vk::Instance& RendererState::GetInstance() { return instance_; }

vk::SurfaceKHR& RendererState::GetSurface() { return surface_; }

vk::PhysicalDevice& RendererState::GetPhysicalDevice()
{
    return physical_device_;
}

vk::Device& RendererState::GetDevice() { return device_; }

vk::CommandPool& RendererState::GetGraphicsCommandPool()
{
    return graphics_command_pool_;
}

vk::Queue& RendererState::GetGraphicsQueue() { return graphics_queue_; }

vk::Queue& RendererState::GetPresentQueue() { return present_queue_; }

vk::CommandPool& RendererState::GetTransientCommandPool()
{
    return transient_command_pool_;
}

vk::Queue& RendererState::GetTransferQueue() { return transfer_queue_; }

TextureCache& RendererState::GetTextureCache() { return texture_cache_; }

MaterialCache& RendererState::GetMaterialCache() { return material_cache_; }

vk::SampleCountFlagBits RendererState::GetMaxSampleCount()
{
    return msaa_samples_;
}

void RendererState::RecreateSwapchain(GLFWwindow* window)
{
    // handle minimization
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    device_.waitIdle();

    device_.destroyImageView(color_image_view_);
    color_image_.reset();
    device_.destroyImageView(depth_image_view_);
    depth_image_.reset();
    
    swapchain_->RecreateSwapchain(*this, window);

    CreateColorResources();
    CreateDepthResources();
}

Swapchain& RendererState::GetSwapchain() { return swapchain_.value(); }

vk::RenderPass& RendererState::GetRenderPass() { return render_pass_; }

vk::DescriptorSetLayout& RendererState::GetDescriptorSetLayout()
{
    return descriptor_set_layout_;
}

vk::ImageView& RendererState::GetColorImageView()
{
    return color_image_view_;
}

vk::ImageView& RendererState::GetDepthImageView()
{
    return depth_image_view_;
}

RendererState::QueueFamilyIndices RendererState::GetQueueFamilies()
{
    return queue_families_;
}

vk::CommandBuffer RendererState::BeginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo alloc_info(
        transient_command_pool_, vk::CommandBufferLevel::ePrimary, 1);

    vk::CommandBuffer command_buffer =
        device_.allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    command_buffer.begin(begin_info);
    return command_buffer;
}

void RendererState::EndSingleTimeCommands(vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    vk::SubmitInfo submit_info({}, {}, command_buffer, {});

    transfer_queue_.submit(submit_info);
    transfer_queue_.waitIdle();

    device_.freeCommandBuffers(transient_command_pool_, command_buffer);
}

bool RendererState::CheckValidationLayerSupport(
    const std::vector<const char*>& layers)
{
    auto supported_validation_layers = vk::enumerateInstanceLayerProperties();

    for (const char* layer : layers) {
        if (std::find_if(supported_validation_layers.begin(),
                         supported_validation_layers.end(), [&layer](auto l) {
                             return strcmp(l.layerName, layer) == 0;
                         }) == supported_validation_layers.end()) {
            return false;
        }
    }

    return true;
}

bool RendererState::CheckInstanceExtensions(
    const std::vector<vk::ExtensionProperties> supported_extensions,
    std::vector<const char*> required_extensions) const
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

vk::Instance RendererState::CreateInstance(
    std::string name, const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& layers)
{
    vk::DynamicLoader dl;
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
        dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    if (!CheckValidationLayerSupport(layers)) {
        throw std::runtime_error("Not all validation layers supported!");
    }

    vk::ApplicationInfo info(ENGINE_NAME.c_str(), VK_MAKE_VERSION(1, 0, 0),
                             "No Engine", VK_MAKE_VERSION(1, 0, 0),
                             VK_API_VERSION_1_2);

    auto extensions = vk::enumerateInstanceExtensionProperties();

    std::cout << "Available extensions:\n";
    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    std::cout << '\n';
    std::cout << "Required extensions:\n";
    for (const auto& extension : required_extensions) {
        std::cout << '\t' << extension << '\n';
    }

    if (!CheckInstanceExtensions(extensions, required_extensions)) {
        throw std::runtime_error("Not all required extensions are available!");
    }

    vk::InstanceCreateInfo create_info(vk::InstanceCreateFlags(), &info, layers,
                                       required_extensions);
    vk::DebugUtilsMessengerCreateInfoEXT messenger_info;

    auto instance = vk::createInstance(create_info);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
    return instance;
}

vk::SurfaceKHR RendererState::CreateSurface(GLFWwindow* window)
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance_, window, nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    return surface;
}

RendererState::QueueFamilyIndices RendererState::FindQueueFamilies(
    const vk::PhysicalDevice& device)
{
    QueueFamilyIndices indices;

    auto queue_families = device.getQueueFamilyProperties();

    uint32_t i = 0;
    for (const auto& queue_family : queue_families) {
        if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphics_family = {i, queue_family};
        }
        if (queue_family.queueFlags & vk::QueueFlagBits::eTransfer) {
            indices.transfer_family = {i, queue_family};
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

RendererState::SwapChainSupportDetails RendererState::QuerySwapChainSupport(
    const vk::PhysicalDevice& device)
{
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
    details.formats = device.getSurfaceFormatsKHR(surface_);
    details.present_modes = device.getSurfacePresentModesKHR(surface_);

    return details;
}

bool RendererState::CheckDeviceExtensionSupport(
    const std::vector<const char*> required_extensions,
    const vk::PhysicalDevice& device)
{
    auto available_extensions = device.enumerateDeviceExtensionProperties();

    for (const auto& extension : required_extensions) {
        if (std::find_if(available_extensions.begin(),
                         available_extensions.end(), [&extension](auto e) {
                             return strcmp(e.extensionName, extension) == 0;
                         }) == available_extensions.end()) {
            return false;
        }
    }

    return true;
}

bool RendererState::IsDeviceSuitable(
    const std::vector<const char*> required_extensions,
    const vk::PhysicalDevice& device)
{
    auto indices = FindQueueFamilies(device);
    bool extensions_supported =
        CheckDeviceExtensionSupport(required_extensions, device);
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

vk::SampleCountFlagBits RendererState::GetMaxUsableSampleCount()
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

vk::PhysicalDevice RendererState::CreatePhysicalDevice(
    const std::vector<const char*> required_extensions)
{
    auto physical_devices = instance_.enumeratePhysicalDevices();
    vk::PhysicalDevice physical_device;
    if (physical_devices.size() == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto& device : physical_devices) {
        if (IsDeviceSuitable(required_extensions, device)) {
            physical_device = device;
            break;
        }
    }

    if (!physical_device) {
        throw std::runtime_error("failed to find a suitable GPU");
    }

    vk::PhysicalDeviceProperties properties = physical_device.getProperties();
    std::cout << "Found device:\n";
    std::cout << '\t' << properties.deviceName << '\n';

    return physical_device;
}

vk::QueueFamilyProperties RendererState::GetQueueFamilyPropertiesByIndex(
    uint32_t index)
{
    auto queue_families = physical_device_.getQueueFamilyProperties();
    return queue_families.at(index);
}

std::tuple<vk::Device, vk::Queue, vk::Queue, vk::Queue>
RendererState::CreateDeviceAndQueues(const std::vector<const char*> extensions,
                                     const std::vector<const char*> layers)
{
    // TODO deal with max queue counts...
    std::unordered_map<uint32_t, uint32_t> index_to_count_map;

    index_to_count_map[queue_families_.graphics_family->index] = 1;
    uint32_t graphics_queue_offset = 0;  // will always be the first

    index_to_count_map[queue_families_.present_family->index] += 1;
    uint32_t present_queue_offset =
        index_to_count_map[queue_families_.present_family->index] - 1;

    index_to_count_map[queue_families_.transfer_family->index] += 1;
    uint32_t transfer_queue_offset =
        index_to_count_map[queue_families_.transfer_family->index] - 1;

    const float priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    for (const auto& index_map_entry : index_to_count_map) {
        vk::QueueFamilyProperties properties =
            GetQueueFamilyPropertiesByIndex(index_map_entry.first);
        vk::DeviceQueueCreateInfo info = {
            vk::DeviceQueueCreateFlags(), index_map_entry.first,
            std::min(index_map_entry.second, properties.queueCount), &priority};
        queue_create_infos.push_back(info);
    };

    vk::PhysicalDeviceFeatures device_features;
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.sampleRateShading = VK_TRUE;

    vk::DeviceCreateInfo create_info;
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = queue_create_infos.size();
    create_info.pEnabledFeatures = &device_features;
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledLayerCount = layers.size();

    auto device = physical_device_.createDevice(create_info);

    vk::Queue graphics_queue = device.getQueue(
        queue_families_.graphics_family->index,
        graphics_queue_offset %
            queue_families_.graphics_family->properties.queueCount);
    vk::Queue present_queue = device.getQueue(
        queue_families_.present_family->index,
        present_queue_offset %
            queue_families_.present_family->properties.queueCount);
    vk::Queue transfer_queue = device.getQueue(
        queue_families_.transfer_family->index,
        transfer_queue_offset %
            queue_families_.transfer_family->properties.queueCount);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
    return {device, graphics_queue, present_queue, transfer_queue};
}

void RendererState::CreateColorResources()
{
    auto extent = swapchain_->GetExtent();
    auto color_format = swapchain_->GetImageFormat().format;

    color_image_.emplace(*this, extent.width, extent.height, 1, msaa_samples_,
                 color_format, vk::ImageTiling::eOptimal,
                 vk::ImageUsageFlagBits::eTransientAttachment |
                     vk::ImageUsageFlagBits::eColorAttachment,
                 vk::MemoryPropertyFlagBits::eDeviceLocal);
    color_image_view_ =
        CreateImageView(*this, color_image_->GetImage(),
                        color_format, vk::ImageAspectFlagBits::eColor, 1);
}

void RendererState::CreateDepthResources()
{
    vk::Format depth_format = FindDepthFormat(physical_device_);

    auto extent = swapchain_->GetExtent();
    depth_image_.emplace(*this, extent.width, extent.height, 1,
                            msaa_samples_, depth_format,
                            vk::ImageTiling::eOptimal,
                            vk::ImageUsageFlagBits::eDepthStencilAttachment,
                            vk::MemoryPropertyFlagBits::eDeviceLocal);

    depth_image_view_ =
        CreateImageView(*this, depth_image_->GetImage(),
                        depth_format, vk::ImageAspectFlagBits::eDepth, 1);
    TransitionImageLayout(*this, depth_image_->GetImage(), depth_format,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);
}

vk::CommandPool RendererState::CreateCommandPool(uint32_t queue_index)
{
    vk::CommandPoolCreateInfo pool_info(vk::CommandPoolCreateFlags(),
                                        queue_index);

    return device_.createCommandPool(pool_info);
}

vk::RenderPass RendererState::CreateRenderPass()
{
    vk::AttachmentDescription color_attachment(
        vk::AttachmentDescriptionFlags(), swapchain_->GetImageFormat().format,
        msaa_samples_, vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare,
        vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depth_attachment(
        vk::AttachmentDescriptionFlags(), FindDepthFormat(physical_device_), msaa_samples_,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentDescription color_attachment_resolve(
        vk::AttachmentDescriptionFlags(), swapchain_->GetImageFormat().format,
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

    return device_.createRenderPass(render_pass_info);
}

vk::DescriptorSetLayout RendererState::CreateDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding ubo_layout_binding(
        0, vk::DescriptorType::eUniformBuffer, 1,
        vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding sampler_layout_binding(
        1, vk::DescriptorType::eCombinedImageSampler, 1,
        vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {
        ubo_layout_binding, sampler_layout_binding};

    vk::DescriptorSetLayoutCreateInfo layout_info(
        vk::DescriptorSetLayoutCreateFlags(), bindings);

    return device_.createDescriptorSetLayout(layout_info);
}