#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "utils.h"

class GpuBuffer
{
public:
    GpuBuffer(vk::Device& device);

    template <typename Container>
    void SetData(vk::PhysicalDevice& physical_device, vk::Device& device,
                 vk::CommandPool& transient_command_pool, vk::Queue& queue,
                 const Container& data, vk::BufferUsageFlags usage,
                 vk::MemoryPropertyFlags properties)
    {
        device_ = device;
        vk::DeviceSize size =
            data.size() * sizeof(typename Container::value_type);
        std::tie(buffer_, memory_) =
            CreateBuffer(device_, physical_device, size, usage, properties);

        TransferDataToGpuBuffer(device_, physical_device,
                                transient_command_pool, queue, buffer_,
                                static_cast<const void*>(data.data()), size);
    }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&& other);

    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer& operator=(GpuBuffer&& other);

    ~GpuBuffer();

    vk::Buffer GetBuffer();

private:
    void Cleanup();
    void MoveFrom(GpuBuffer&& other);
    vk::Device& device_;

    vk::Buffer buffer_;
    vk::DeviceMemory memory_;
};