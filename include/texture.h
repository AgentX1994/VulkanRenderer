#pragma once

#include "gpu_image.h"
#include "stb_image.h"

class RendererState;

class Texture
{
public:
    Texture(RendererState& renderer,
            std::string texture_path);

    Texture(const Texture&) = delete;
    Texture(Texture&& other);

    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&& other);

    ~Texture();

    vk::Image GetImage();
    vk::ImageView GetImageView();

private:
    void MoveFrom(Texture&& other);
    vk::Device& device_;

    GpuImage image_;
    vk::ImageView image_view_;
};