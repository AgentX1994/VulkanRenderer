#include "utils.h"

#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>

#include "common.h"
#include "common_vulkan.h"

#include "renderer_state.h"

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

const bool CheckExtensions(
    const std::vector<vk::ExtensionProperties> supported_extensions,
    std::vector<const char*> required_extensions)
{
    for (const auto extension_name : required_extensions) {
        if (std::find_if(supported_extensions.begin(),
                         supported_extensions.end(), [&extension_name](auto e) {
                             return strcmp(e.extensionName, extension_name) ==
                                    0;
                         }) == supported_extensions.end()) {
            return false;
        }
    }
    return true;
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
    RendererState& renderer, vk::DeviceSize size, vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    auto device = renderer.GetDevice();
    auto physical_device = renderer.GetPhysicalDevice();

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
    RendererState& renderer, uint32_t width, uint32_t height,
    uint32_t mip_levels, vk::SampleCountFlagBits num_samples, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    auto device = renderer.GetDevice();
    auto physical_device = renderer.GetPhysicalDevice();

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

vk::ImageView CreateImageView(RendererState& renderer, vk::Image image,
                              vk::Format format,
                              vk::ImageAspectFlags aspect_flags,
                              uint32_t mip_levels)
{
    auto device = renderer.GetDevice();

    vk::ImageViewCreateInfo view_info(
        vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, format,
        vk::ComponentSwizzle(),
        vk::ImageSubresourceRange(aspect_flags, 0, mip_levels, 0, 1));

    return device.createImageView(view_info);
}

void TransferDataToGpuBuffer(RendererState& renderer, vk::Buffer buffer,
                             const void* data, vk::DeviceSize size)
{
    auto [staging_buffer, staging_buffer_memory] =
        CreateBuffer(renderer, size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dest = renderer.GetDevice().mapMemory(staging_buffer_memory, 0, size);
    memcpy(dest, data, (size_t)size);
    renderer.GetDevice().unmapMemory(staging_buffer_memory);

    CopyBuffer(renderer, staging_buffer, buffer, size);

    renderer.GetDevice().destroyBuffer(staging_buffer);
    renderer.GetDevice().freeMemory(staging_buffer_memory);
}

void TransferDataToGpuImage(RendererState& renderer, uint32_t width,
                            uint32_t height, vk::Image image, const void* data,
                            vk::DeviceSize size)
{
    auto [staging_buffer, staging_buffer_memory] =
        CreateBuffer(renderer, size, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void* dest = renderer.GetDevice().mapMemory(staging_buffer_memory, 0, size);
    memcpy(dest, data, (size_t)size);
    renderer.GetDevice().unmapMemory(staging_buffer_memory);

    CopyBufferToImage(renderer, staging_buffer, image, width, height);

    renderer.GetDevice().destroyBuffer(staging_buffer);
    renderer.GetDevice().freeMemory(staging_buffer_memory);
}

void CopyBuffer(RendererState& renderer, vk::Buffer src, vk::Buffer dst,
                vk::DeviceSize size)
{
    vk::BufferCopy copy_info(0, 0, size);
    auto transfer_command_buffer = renderer.BeginSingleTimeCommands();
    transfer_command_buffer.copyBuffer(src, dst, copy_info);
    renderer.EndSingleTimeCommands(transfer_command_buffer);
}

void CopyBufferToImage(RendererState& renderer, vk::Buffer buffer,
                       vk::Image image, uint32_t width, uint32_t height)
{
    auto command_buffer = renderer.BeginSingleTimeCommands();

    vk::BufferImageCopy region(
        0, 0, 0,
        vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
        {0, 0, 0}, {width, height, 1});

    command_buffer.copyBufferToImage(
        buffer, image, vk::ImageLayout::eTransferDstOptimal, region);

    renderer.EndSingleTimeCommands(command_buffer);
}

void TransitionImageLayout(RendererState& renderer, vk::Image image,
                           vk::Format format, vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout, uint32_t mip_levels)
{
    auto command_buffer = renderer.BeginSingleTimeCommands();

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

    renderer.EndSingleTimeCommands(command_buffer);
}

void GenerateMipMaps(RendererState& renderer, vk::Image image,
                     vk::Format format, int32_t texture_width,
                     int32_t texture_height, uint32_t mip_levels)
{
    auto format_properties =
        renderer.GetPhysicalDevice().getFormatProperties(format);
    if (!(format_properties.optimalTilingFeatures &
          vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
        throw std::runtime_error(
            "texture image format does not support linear blitting!");
    }

    auto command_buffer = renderer.BeginSingleTimeCommands();

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

    renderer.EndSingleTimeCommands(command_buffer);
}

bool HasStencilComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint ||
           format == vk::Format::eD24UnormS8Uint;
}

vk::Format FindSupportedFormat(vk::PhysicalDevice& physical_device,
    const std::vector<vk::Format>& candidates, vk::ImageTiling tiling,
    vk::FormatFeatureFlags features)
{
    for (auto format : candidates) {
        auto props = physical_device.getFormatProperties(format);
        switch (tiling) {
            case vk::ImageTiling::eLinear:
                if ((props.linearTilingFeatures & features) == features) {
                    return format;
                }
                break;
            case vk::ImageTiling::eOptimal:
                if ((props.optimalTilingFeatures & features) == features) {
                    return format;
                }
                break;
            case vk::ImageTiling::eDrmFormatModifierEXT:
                // TODO What is this
                break;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format FindDepthFormat(vk::PhysicalDevice& physical_device)
{
    return FindSupportedFormat(physical_device, 
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}


glm::quat RotationBetweenVectors(glm::vec3 v1, glm::vec3 v2)
{
    // Method from http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-17-quaternions/
    v1 = glm::normalize(v1);
    v2 = glm::normalize(v2);
    
    float cos_theta = glm::dot(v1, v2);
    glm::vec3 rotation_axis;

    if (cos_theta < -1 + 0.001f) {
        // v1 and v2 are almost in opposite directions
        // In this case, any axis will work, there's no
        // ideal axis. Just guess a perpendicular axis
        rotation_axis = glm::cross(glm::vec3(0.0, 0.0, 1.0f), v1);
        if (glm::length2(rotation_axis) < 0.01f) {
            // bad luck, v1 and 0,0,1 were parallel
			rotation_axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), v1);
        }
        rotation_axis = glm::normalize(rotation_axis);
        return glm::angleAxis(glm::radians(180.0f), rotation_axis);
    }
    // I'm not sure why this needs to be normalized, but it does
    rotation_axis = glm::normalize(glm::cross(v1, v2));

	//float s = sqrt( (1+cosTheta)*2 );
	//float invs = 1 / s;

	//return quat(
	//	s * 0.5f, 
	//	rotationAxis.x * invs,
	//	rotationAxis.y * invs,
	//	rotationAxis.z * invs
	//);

    // we have cos_theta, but we want
    // cos_half_theta
    // sin_half_theta
    // use the half angle formula for sin and cos

    float sin_half_theta = glm::sqrt((1 - cos_theta) * 0.5f);
    float cos_half_theta = glm::sqrt((1 + cos_theta) * 0.5f);

    return glm::quat(
        cos_half_theta,
        rotation_axis.x * sin_half_theta,
        rotation_axis.y * sin_half_theta,
        rotation_axis.z * sin_half_theta
    );
}

glm::quat QuaternionLookAt(glm::vec3 position, glm::vec3 point, glm::vec3 up)
{
    glm::vec3 dir = glm::normalize(point - position);
    up = glm::normalize(up);
    glm::quat q = glm::quatLookAt(dir, up);
    return q;
}