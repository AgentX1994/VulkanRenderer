#include "texture_cache.h"

void TextureCache::LoadTexture(vk::PhysicalDevice physical_device,
                               vk::Device& device,
                               vk::CommandPool& transient_command_pool,
                               vk::Queue& queue, std::string path)
{
    if (texture_map_.find(path) != texture_map_.end()) {
        texture_map_.emplace(std::move(path), Texture(physical_device, device,
                                    transient_command_pool, queue, path));
    }
}

const Texture* TextureCache::GetTextureByPath(std::string path) {
    if (texture_map_.find(path) != texture_map_.end())
    {
        return &texture_map_.at(path);
    }
    return nullptr;
}