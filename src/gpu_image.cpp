#include "gpu_image.h"

#include "utils.h"

GpuImage::GpuImage(vk::Device& device) : device_(device){};

GpuImage::GpuImage(GpuImage&& other) : device_(other.device_)
{
    MoveFrom(std::move(other));
}

GpuImage& GpuImage::operator=(GpuImage&& other)
{
    MoveFrom(std::move(other));
    return *this;
}

GpuImage::~GpuImage() { Cleanup(); }

void GpuImage::SetData(vk::PhysicalDevice& physical_device, vk::Device& device,
                       vk::CommandPool& transient_command_pool,
                       vk::Queue& queue, uint32_t width, uint32_t height,
                       const uint8_t* data, uint32_t mip_levels,
                       vk::SampleCountFlagBits num_samples, vk::Format format,
                       vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                       vk::MemoryPropertyFlags properties)
{
    device_ = device;
    vk::DeviceSize size = width * height * 4;
    std::tie(image_, memory_) =
        CreateImage(device_, physical_device, width, height, mip_levels,
                    num_samples, format, tiling, usage, properties);

    TransitionImageLayout(device, transient_command_pool, queue, image_,
                          vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal, mip_levels);

    TransferDataToGpuImage(device_, physical_device, transient_command_pool,
                           queue, width, height, image_,
                           static_cast<const void*>(data), size);
}

void GpuImage::Cleanup()
{
    if (image_) {
        device_.destroyImage(image_);
    }
    if (memory_) {
        device_.freeMemory(memory_);
    }
}

void GpuImage::MoveFrom(GpuImage&& other)
{
    Cleanup();
    device_ = other.device_;
    image_ = other.image_;
    other.image_ = (VkImage)VK_NULL_HANDLE;
    memory_ = other.memory_;
    other.memory_ = (VkDeviceMemory)VK_NULL_HANDLE;
}

vk::Image GpuImage::GetImage() { return image_; }