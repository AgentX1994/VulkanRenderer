#pragma once

#include <array>
#include <vector>

#include "common.h"
#include "common_vulkan.h"
#include "renderer_state.h"
#include "utils.h"

class GpuBuffer
{
public:
    GpuBuffer(vk::Device& device);
    GpuBuffer(RendererState& renderer, vk::DeviceSize buffer_size,
              vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

    template <typename Container>
    void SetData(RendererState& renderer, const Container& data,
                 vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
    {
        device_ = renderer.GetDevice();
        vk::DeviceSize size =
            data.size() * sizeof(typename Container::value_type);
        std::tie(buffer_, memory_) =
            CreateBuffer(renderer, size, usage, properties);

        TransferDataToGpuBuffer(renderer, buffer_,
                                static_cast<const void*>(data.data()), size);
    }

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer(GpuBuffer&& other);

    GpuBuffer& operator=(const GpuBuffer&) = delete;
    GpuBuffer& operator=(GpuBuffer&& other);

    ~GpuBuffer();

    vk::Buffer GetBuffer() const;
    vk::DeviceMemory GetMemory() const;

private:
    void Cleanup();
    void MoveFrom(GpuBuffer&& other);
    vk::Device& device_;

    vk::Buffer buffer_;
    vk::DeviceMemory memory_;
};