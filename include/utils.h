#pragma once

#include <shaderc/shaderc.hpp>
#include <string>

#include "common.h"
#include "common_vulkan.h"

class RendererState;

std::string GetFileContents(const char* filename);

const std::string CompilationStatusToString(shaderc_compilation_status status);

std::vector<uint32_t> CompileShader(const std::string& path,
                                    shaderc_shader_kind kind);

uint32_t FindMemoryType(vk::PhysicalDevice& physical_device,
                        uint32_t type_filter,
                        vk::MemoryPropertyFlags properties);

std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(
    RendererState& renderer, vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties);

std::pair<vk::Image, vk::DeviceMemory> CreateImage(
    RendererState& renderer, uint32_t width, uint32_t height,
    uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties);

vk::ImageView CreateImageView(RendererState& renderer, vk::Image image,
                              vk::Format format,
                              vk::ImageAspectFlags aspect_flags,
                              uint32_t mip_levels);

void TransferDataToGpuBuffer(RendererState& renderer, vk::Buffer buffer,
                             const void* data, vk::DeviceSize size);

void TransferDataToGpuImage(RendererState& renderer, uint32_t width,
                            uint32_t height, vk::Image image, const void* data,
                            vk::DeviceSize size);

void CopyBuffer(RendererState& renderer, vk::Buffer src, vk::Buffer dest,
                vk::DeviceSize size);

void CopyBufferToImage(RendererState& renderer, vk::Buffer buffer,
                       vk::Image image, uint32_t width, uint32_t height);

void TransitionImageLayout(RendererState& renderer, vk::Image image,
                           vk::Format format, vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout, uint32_t mip_levels);

void GenerateMipMaps(RendererState& renderer, vk::Image image,
                     vk::Format format, int32_t texture_width,
                     int32_t texture_height, uint32_t mip_levels);

bool HasStencilComponent(vk::Format format);

vk::Format FindSupportedFormat(vk::PhysicalDevice& physical_device,
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features);

vk::Format FindDepthFormat(vk::PhysicalDevice& physical_device);