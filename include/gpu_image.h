#pragma once

#include <array>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "utils.h"

class GpuImage
{
public:
    GpuImage(vk::Device& device);

    GpuImage(const GpuImage&) = delete;
    GpuImage(GpuImage&& other);

    GpuImage& operator=(const GpuImage&) = delete;
    GpuImage& operator=(GpuImage&& other);

    ~GpuImage();

    void SetData(vk::PhysicalDevice& physical_device, vk::Device& device,
                 vk::CommandPool& transient_command_pool, vk::Queue& queue,
                 uint32_t width, uint32_t height, const uint8_t* data,
                 uint32_t mip_levels, vk::SampleCountFlagBits num_samples,
                 vk::Format format, vk::ImageTiling tiling,
                 vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

    vk::Image GetImage();

private:
    void Cleanup();
    void MoveFrom(GpuImage&& other);
    vk::Device& device_;

    vk::Image image_;
    vk::DeviceMemory memory_;
};