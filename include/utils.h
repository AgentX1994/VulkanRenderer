#pragma once

#include <shaderc/shaderc.hpp>
#include <string>
#include <vulkan/vulkan.hpp>

std::string GetFileContents(const char* filename);

const std::string CompilationStatusToString(shaderc_compilation_status status);

std::vector<uint32_t> CompileShader(const std::string& path,
                                    shaderc_shader_kind kind);

uint32_t FindMemoryType(vk::PhysicalDevice& physical_device,
                        uint32_t type_filter,
                        vk::MemoryPropertyFlags properties);

std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(
    vk::Device& device, vk::PhysicalDevice& physical_device,
    vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties);

std::pair<vk::Image, vk::DeviceMemory> CreateImage(
    vk::Device& device, vk::PhysicalDevice& physical_device, uint32_t width,
    uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples,
    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties);

vk::ImageView CreateImageView(vk::Device& device, vk::Image image,
                              vk::Format format,
                              vk::ImageAspectFlags aspect_flags,
                              uint32_t mip_levels);

void TransferDataToGpuBuffer(vk::Device& device,
                             vk::PhysicalDevice& physical_device,
                             vk::CommandPool& transient_command_pool,
                             vk::Queue& queue, vk::Buffer buffer,
                             const void* data, vk::DeviceSize size);

void TransferDataToGpuImage(vk::Device& device,
                            vk::PhysicalDevice& physical_device,
                            vk::CommandPool& transient_command_pool,
                            vk::Queue& queue, uint32_t width, uint32_t height,
                            vk::Image image, const void* data,
                            vk::DeviceSize size);

void CopyBuffer(vk::Device& device, vk::CommandPool& transient_command_pool,
                vk::Queue& queue, vk::Buffer src, vk::Buffer dest,
                vk::DeviceSize size);

void CopyBufferToImage(vk::Device& device,
                       vk::CommandPool& transient_command_pool,
                       vk::Queue& queue, vk::Buffer buffer, vk::Image image,
                       uint32_t width, uint32_t height);

void TransitionImageLayout(vk::Device& device,
                           vk::CommandPool& transient_command_pool,
                           vk::Queue& queue, vk::Image image, vk::Format format,
                           vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout, uint32_t mip_levels);

void GenerateMipMaps(vk::PhysicalDevice& physical_device, vk::Device& device,
                     vk::CommandPool& transient_command_pool, vk::Queue& queue,
                     vk::Image image, vk::Format format, int32_t texture_width,
                     int32_t texture_height, uint32_t mip_levels);

vk::CommandBuffer BeginSingleTimeCommands(
    vk::Device& device, vk::CommandPool& transient_command_pool);

void EndSingleTimeCommands(vk::Device& device,
                           vk::CommandPool& transient_command_pool,
                           vk::CommandBuffer command_buffer, vk::Queue& queue);

bool HasStencilComponent(vk::Format format);