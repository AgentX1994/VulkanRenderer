#include "texture_cache.h"

#include "texture.h"

void TextureCache::LoadTexture(RendererState& renderer, std::string path)
{
    if (texture_map_.find(path) == texture_map_.end()) {
        texture_map_.emplace(std::move(path), Texture(renderer, path));
    }
}

Texture* TextureCache::GetTextureByPath(std::string path) {
    if (texture_map_.find(path) != texture_map_.end())
    {
        return &texture_map_.at(path);
    }
    return nullptr;
}

void TextureCache::Clear()
{
    texture_map_.clear();
}