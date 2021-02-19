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

std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(vk::Device& device,
    vk::PhysicalDevice& physical_device, vk::DeviceSize size,
    vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

void TransferDataToGpuBuffer(vk::Device& device, vk::PhysicalDevice& physical_device,
                                vk::CommandPool& transient_command_pool,
                                vk::Queue& queue, vk::Buffer buffer, const void* data,
                                vk::DeviceSize size);

void CopyBuffer(vk::Device& device, vk::CommandPool& transient_command_pool, vk::Queue& queue,
                vk::Buffer src, vk::Buffer dest, vk::DeviceSize size);

vk::CommandBuffer BeginSingleTimeCommands(
    vk::Device& device, vk::CommandPool& transient_command_pool);

void EndSingleTimeCommands(vk::Device& device,
                           vk::CommandPool& transient_command_pool,
                           vk::CommandBuffer command_buffer, vk::Queue& queue);