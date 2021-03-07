#pragma once

#include <map>
#include <string>

#include "common.h"

#include "texture.h"

class RendererState;

class TextureCache
{
public:
    void LoadTexture(RendererState& renderer,
                     std::string path);
    
    Texture* GetTextureByPath(std::string path);

    void Clear();
    
private:
    std::map<std::string, Texture> texture_map_;
};