#include "texture.h"

#include <cmath>

Texture::Texture(vk::PhysicalDevice& physical_device, vk::Device& device,
                 vk::CommandPool& transient_command_pool, vk::Queue& queue,
                 std::string texture_path)
    : device_(device), image_(device)
{
    int texture_width, texture_height, texture_channels;
    stbi_uc* pixels =
        stbi_load(texture_path.c_str(), &texture_width, &texture_height,
                  &texture_channels, STBI_rgb_alpha);
    vk::DeviceSize image_size = texture_width * texture_height * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image");
    }

    uint32_t mip_levels =
        static_cast<uint32_t>(
            std::floor(std::log2(std::max(texture_width, texture_height)))) +
        1;

    image_.SetData(physical_device, device, transient_command_pool, queue,
                   texture_width, texture_height, pixels, mip_levels,
                   vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb,
                   vk::ImageTiling::eOptimal,
                   vk::ImageUsageFlagBits::eTransferSrc |
                       vk::ImageUsageFlagBits::eTransferDst |
                       vk::ImageUsageFlagBits::eSampled,
                   vk::MemoryPropertyFlagBits::eDeviceLocal);

    stbi_image_free(pixels);

    // will transition while generating mipmaps
    //   TransitionImageLayout(texture_image_, vk::Format::eR8G8B8A8Srgb,
    //                         vk::ImageLayout::eTransferDstOptimal,
    //                         vk::ImageLayout::eShaderReadOnlyOptimal,
    //                         mip_levels_);
    GenerateMipMaps(physical_device, device, transient_command_pool, queue,
                    image_.GetImage(), vk::Format::eR8G8B8A8Srgb, texture_width,
                    texture_height, mip_levels);

    image_view_ =
        CreateImageView(device, image_.GetImage(), vk::Format::eR8G8B8A8Srgb,
                        vk::ImageAspectFlagBits::eColor, mip_levels);
}

Texture::Texture(Texture&& other)
    : device_(other.device_), image_(other.device_)
{
    MoveFrom(std::move(other));
}

Texture& Texture::operator=(Texture&& other)
{
    device_.destroyImageView(image_view_);
    MoveFrom(std::move(other));
    return *this;
}

Texture::~Texture() { device_.destroyImageView(image_view_); }

vk::Image Texture::GetImage() { return image_.GetImage(); }

vk::ImageView Texture::GetImageView() { return image_view_; }

void Texture::MoveFrom(Texture&& other)
{
    device_ = other.device_;
    image_ = std::move(other.image_);
    image_view_ = std::move(other.image_view_);
    other.image_view_ = (VkImageView)VK_NULL_HANDLE;
}