#pragma once

#include <map>
#include <optional>

#include <vulkan/vulkan.hpp>

#include "texture.h"

class TextureCache
{
public:
    void LoadTexture(vk::PhysicalDevice physical_device, vk::Device& device,
                     vk::CommandPool& transient_command_pool, vk::Queue& queue,
                     std::string path);
    
    const Texture* GetTextureByPath(std::string path);

private:
    std::map<std::string, Texture> texture_map_;
};