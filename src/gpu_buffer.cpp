#include "gpu_buffer.h"

#include "utils.h"

GpuBuffer::GpuBuffer(vk::Device& device) : device_(device){};

GpuBuffer::GpuBuffer(RendererState& renderer, vk::DeviceSize buffer_size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
: device_(renderer.GetDevice())
{
    std::tie(buffer_, memory_) =
        CreateBuffer(renderer, buffer_size, usage, properties);
}

GpuBuffer::GpuBuffer(GpuBuffer&& other) : device_(other.device_)
{
    MoveFrom(std::move(other));
}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other)
{
    MoveFrom(std::move(other));
    return *this;
}

GpuBuffer::~GpuBuffer() { Cleanup(); }

void GpuBuffer::Cleanup()
{
    if (buffer_) {
        device_.destroyBuffer(buffer_);
    }
    if (memory_) {
        device_.freeMemory(memory_);
    }
}

void GpuBuffer::MoveFrom(GpuBuffer&& other)
{
    Cleanup();
    device_ = other.device_;
    buffer_ = other.buffer_;
    other.buffer_ = (VkBuffer)VK_NULL_HANDLE;
    memory_ = other.memory_;
    other.memory_ = (VkDeviceMemory)VK_NULL_HANDLE;
}

vk::Buffer GpuBuffer::GetBuffer() { return buffer_; }

vk::DeviceMemory GpuBuffer::GetMemory() { return memory_; }