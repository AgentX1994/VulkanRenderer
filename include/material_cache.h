#pragma once

#include <map>
#include <string>

#include "tiny_obj_loader.h"
#include "material.h"

class RendererState;

class MaterialCache
{
public:
    void LoadMaterial(RendererState& renderer,
                     std::string name, tinyobj::material_t material_definition);
    
    Material* GetMaterialByName(std::string name);

    void Clear();

private:
    std::map<std::string, Material> material_map_;
};