#include "utils.h"

#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <vulkan/vulkan.hpp>

std::string GetFileContents(const char* filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return (contents);
    }
    throw(errno);
}

const std::string CompilationStatusToString(shaderc_compilation_status status)
{
    switch (status) {
        case shaderc_compilation_status_success:
            return "Success";
            break;
        case shaderc_compilation_status_invalid_stage:
            return "Invalid Stage Error";
            break;
        case shaderc_compilation_status_compilation_error:
            return "Compilation Error";
            break;
        case shaderc_compilation_status_internal_error:
            return "Internal Error";
            break;
        case shaderc_compilation_status_null_result_object:
            return "Null Result Error";
            break;
        case shaderc_compilation_status_invalid_assembly:
            return "Invalid Assembly Error";
            break;
        case shaderc_compilation_status_validation_error:
            return "Validation Error";
            break;
        case shaderc_compilation_status_transformation_error:
            return "Transformation Error";
            break;
        case shaderc_compilation_status_configuration_error:
            return "Configuration Error";
            break;
        default:
            return "Unknown Error";
            break;
    }
}

std::vector<uint32_t> CompileShader(const std::string& path,
                                    shaderc_shader_kind kind)
{
    std::string shader_source = GetFileContents(path.c_str());
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    shaderc::SpvCompilationResult result =
        compiler.CompileGlslToSpv(shader_source, kind, path.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        // handle errors
        shaderc_compilation_status status = result.GetCompilationStatus();
        std::string error_type = CompilationStatusToString(status);
        std::cerr << error_type << ":\n"
                  << result.GetErrorMessage() << std::endl;
        assert(false);
    }
    std::vector<uint32_t> vertexSPRV;
    vertexSPRV.assign(result.cbegin(), result.cend());
    return vertexSPRV;
}

uint32_t FindMemoryType(vk::PhysicalDevice& physical_device,
                        uint32_t type_filter,
                        vk::MemoryPropertyFlags properties)
{
    auto mem_properties = physical_device.getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

std::pair<vk::Buffer, vk::DeviceMemory> CreateBuffer(
    vk::Device& device, vk::PhysicalDevice& physical_device,
    vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo buffer_info(vk::BufferCreateFlags(), size, usage,
                                     vk::SharingMode::eExclusive);

    auto buffer = device.createBuffer(buffer_info);

    auto mem_reqs = device.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size,
        FindMemoryType(physical_device, mem_reqs.memoryTypeBits, properties));

    auto memory = device.allocateMemory(alloc_info);
    device.bindBufferMemory(buffer, memory, 0);

    return {buffer, memory};
}

void TransferDataToGpuBuffer(vk::Device& device,
                             vk::PhysicalDevice& physical_device,
                             vk::CommandPool& transient_command_pool,
                             vk::Queue& queue, vk::Buffer buffer,
                             const void* data, vk::DeviceSize size)
{
    auto [staging_buffer, staging_buffer_memory] = CreateBuffer(
        device, physical_device, size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dest = device.mapMemory(staging_buffer_memory, 0, size);
    memcpy(dest, data, (size_t)size);
    device.unmapMemory(staging_buffer_memory);

    CopyBuffer(device, transient_command_pool, queue, staging_buffer, buffer,
               size);

    device.destroyBuffer(staging_buffer);
    device.freeMemory(staging_buffer_memory);
}

void CopyBuffer(vk::Device& device, vk::CommandPool& transient_command_pool,
                vk::Queue& queue, vk::Buffer src, vk::Buffer dst,
                vk::DeviceSize size)
{
    vk::BufferCopy copy_info(0, 0, size);
    auto transfer_command_buffer =
        BeginSingleTimeCommands(device, transient_command_pool);
    transfer_command_buffer.copyBuffer(src, dst, copy_info);
    EndSingleTimeCommands(device, transient_command_pool,
                          transfer_command_buffer, queue);
}

vk::CommandBuffer BeginSingleTimeCommands(
    vk::Device& device, vk::CommandPool& transient_command_pool)
{
    vk::CommandBufferAllocateInfo alloc_info(
        transient_command_pool, vk::CommandBufferLevel::ePrimary, 1);

    vk::CommandBuffer command_buffer =
        device.allocateCommandBuffers(alloc_info)[0];

    vk::CommandBufferBeginInfo begin_info(
        vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    command_buffer.begin(begin_info);
    return command_buffer;
}

void EndSingleTimeCommands(vk::Device& device,
                           vk::CommandPool& transient_command_pool,
                           vk::CommandBuffer command_buffer, vk::Queue& queue)
{
    command_buffer.end();

    vk::SubmitInfo submit_info({}, {}, command_buffer, {});

    queue.submit(submit_info);
    queue.waitIdle();

    device.freeCommandBuffers(transient_command_pool, command_buffer);
}