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

std::pair<vk::Image, vk::DeviceMemory> CreateImage(
    vk::Device& device, vk::PhysicalDevice& physical_device, uint32_t width,
    uint32_t height, uint32_t mip_levels, vk::SampleCountFlagBits num_samples,
    vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::DeviceSize image_size = width * height * 4;

    vk::ImageCreateInfo image_info(
        vk::ImageCreateFlags(), vk::ImageType::e2D, format, {width, height, 1},
        mip_levels, 1, num_samples, tiling, usage, vk::SharingMode::eExclusive,
        {}, vk::ImageLayout::eUndefined);

    vk::Image image = device.createImage(image_info);

    vk::MemoryRequirements mem_reqs = device.getImageMemoryRequirements(image);

    vk::MemoryAllocateInfo alloc_info(
        mem_reqs.size,
        FindMemoryType(physical_device, mem_reqs.memoryTypeBits, properties));

    vk::DeviceMemory memory = device.allocateMemory(alloc_info);
    device.bindImageMemory(image, memory, 0);

    return {image, memory};
}

vk::ImageView CreateImageView(vk::Device& device, vk::Image image,
                              vk::Format format,
                              vk::ImageAspectFlags aspect_flags,
                              uint32_t mip_levels)
{
    vk::ImageViewCreateInfo view_info(
        vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, format,
        vk::ComponentSwizzle(),
        vk::ImageSubresourceRange(aspect_flags, 0, mip_levels, 0, 1));

    return device.createImageView(view_info);
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

void TransferDataToGpuImage(vk::Device& device,
                            vk::PhysicalDevice& physical_device,
                            vk::CommandPool& transient_command_pool,
                            vk::Queue& queue, uint32_t width, uint32_t height,
                            vk::Image image, const void* data,
                            vk::DeviceSize size)
{
    auto [staging_buffer, staging_buffer_memory] = CreateBuffer(
        device, physical_device, size, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dest = device.mapMemory(staging_buffer_memory, 0, size);
    memcpy(dest, data, (size_t)size);
    device.unmapMemory(staging_buffer_memory);

    CopyBufferToImage(device, transient_command_pool, queue, staging_buffer,
                      image, width, height);

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

void CopyBufferToImage(vk::Device& device,
                       vk::CommandPool& transient_command_pool,
                       vk::Queue& queue, vk::Buffer buffer, vk::Image image,
                       uint32_t width, uint32_t height)
{
    auto command_buffer =
        BeginSingleTimeCommands(device, transient_command_pool);

    vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {width, height, 1});

    command_buffer.copyBufferToImage(
        buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    EndSingleTimeCommands(device, transient_command_pool, command_buffer,
                          queue);
}

void TransitionImageLayout(vk::Device& device,
                           vk::CommandPool& transient_command_pool,
                           vk::Queue& queue, vk::Image image, vk::Format format,
                           vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout, uint32_t mip_levels)
{
    auto command_buffer =
        BeginSingleTimeCommands(device, transient_command_pool);

    vk::AccessFlags source_access_mask;
    vk::AccessFlags destination_access_mask;
    vk::PipelineStageFlags source_stage;
    vk::PipelineStageFlags destination_stage;

    if (old_layout == vk::ImageLayout::eUndefined &&
        new_layout == vk::ImageLayout::eTransferDstOptimal) {
        source_access_mask = vk::AccessFlags(0);
        destination_access_mask = vk::AccessFlagBits::eTransferWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal &&
               new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        source_access_mask = vk::AccessFlagBits::eTransferWrite;
        destination_access_mask = vk::AccessFlagBits::eShaderRead;
        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eUndefined &&
               new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        source_access_mask = vk::AccessFlags(0);
        destination_access_mask =
            vk::AccessFlagBits::eDepthStencilAttachmentRead |
            vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vk::ImageMemoryBarrier barrier(
        source_access_mask, destination_access_mask, old_layout, new_layout,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0,
                                  mip_levels, 0, 1));

    if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if (HasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |=
                vk::ImageAspectFlagBits::eStencil;
        }
    }

    command_buffer.pipelineBarrier(source_stage, destination_stage,
                                   vk::DependencyFlags(), {}, {}, barrier);

    EndSingleTimeCommands(device, transient_command_pool, command_buffer,
                          queue);
}

void GenerateMipMaps(vk::PhysicalDevice& physical_device, vk::Device& device,
                     vk::CommandPool& transient_command_pool, vk::Queue& queue,
                     vk::Image image, vk::Format format, int32_t texture_width,
                     int32_t texture_height, uint32_t mip_levels)
{
    auto format_properties = physical_device.getFormatProperties(format);
    if (!(format_properties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error(
            "texture image format does not support linear blitting!");
    }

    auto command_buffer =
        BeginSingleTimeCommands(device, transient_command_pool);

    vk::ImageMemoryBarrier barrier(
        vk::AccessFlags(0), vk::AccessFlags(0), vk::ImageLayout::eUndefined,
        vk::ImageLayout::eUndefined, VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED, image,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
    int32_t mip_width = texture_width;
    int32_t mip_height = texture_height;

    for (uint32_t i = 1; i < mip_levels; ++i) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                       vk::PipelineStageFlagBits::eTransfer, {},
                                       {}, {}, barrier);

        vk::ImageBlit blit;
        blit.srcOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.srcOffsets[1] = vk::Offset3D(mip_width, mip_height, 1);
        blit.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = vk::Offset3D(0, 0, 0);
        blit.dstOffsets[1] =
            vk::Offset3D(mip_width > 1 ? mip_width / 2 : 1,
                         mip_height > 1 ? mip_height / 2 : 1, 1);
        blit.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        command_buffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
                                 image, vk::ImageLayout::eTransferDstOptimal,
                                 blit, vk::Filter::eLinear);

        barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        command_buffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

        if (mip_width > 1) {
            mip_width /= 2;
        }
        if (mip_height > 1) {
            mip_height /= 2;
        }
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

    command_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                   vk::PipelineStageFlagBits::eFragmentShader,
                                   {}, {}, {}, barrier);

    EndSingleTimeCommands(device, transient_command_pool, command_buffer,
                          queue);
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

bool HasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint ||
           format == vk::Format::eD24UnormS8Uint;
}