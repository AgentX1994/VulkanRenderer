#pragma once

#include <map>
#include <optional>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "texture.h"

class RendererState;

class TextureCache
{
public:
    void LoadTexture(RendererState& renderer,
                     std::string path);
    
    const Texture* GetTextureByPath(std::string path);

private:
    std::map<std::string, Texture> texture_map_;
};