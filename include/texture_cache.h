#pragma once

#include <map>
#include <string>

#include "texture.h"

class RendererState;

class TextureCache
{
public:
    void LoadTexture(RendererState& renderer,
                     std::string path);
    
    const Texture* GetTextureByPath(std::string path);

    void Clear();
    
private:
    std::map<std::string, Texture> texture_map_;
};