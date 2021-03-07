#pragma once

#include <array>
#include <vector>

#include "common.h"
#include "common_vulkan.h"

#include "utils.h"

class RendererState;

class GpuImage
{
public:
    GpuImage(vk::Device& device);
    GpuImage(RendererState& renderer, uint32_t width, uint32_t height,
              uint32_t mip_levels, vk::SampleCountFlagBits num_samples,
              vk::Format format, vk::ImageTiling tiling,
              vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

    GpuImage(const GpuImage&) = delete;
    GpuImage(GpuImage&& other);

    GpuImage& operator=(const GpuImage&) = delete;
    GpuImage& operator=(GpuImage&& other);

    ~GpuImage();

    void SetData(RendererState& renderer, uint32_t width, uint32_t height,
                 const uint8_t* data, uint32_t mip_levels,
                 vk::SampleCountFlagBits num_samples, vk::Format format,
                 vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                 vk::MemoryPropertyFlags properties);

    vk::Image GetImage();

private:
    void Cleanup();
    void MoveFrom(GpuImage&& other);
    vk::Device& device_;

    vk::Image image_;
    vk::DeviceMemory memory_;
};