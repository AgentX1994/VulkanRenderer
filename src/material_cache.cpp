#include "material_cache.h"

#include "material.h"

void MaterialCache::LoadMaterial(RendererState& renderer,
                     std::string name, tinyobj::material_t material_definition)
{
    if (material_map_.find(name) == material_map_.end()) {
        material_map_.emplace(std::move(name), Material(renderer, material_definition));
    }
}

Material* MaterialCache::GetMaterialByName(std::string name) {
    if (material_map_.find(name) != material_map_.end())
    {
        return &material_map_.at(name);
    }
    return nullptr;
}

void MaterialCache::Clear()
{
    material_map_.clear();
}