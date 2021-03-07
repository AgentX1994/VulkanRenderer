#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "common_vulkan.h"

#include "mesh.h"
#include "tiny_obj_loader.h"

class Model
{
public:
    Model(RendererState& renderer, std::string path);

    void RecordDrawCommand(RendererState& renderer,
                           vk::CommandBuffer& command_buffer,
                           /* temporary */ vk::DescriptorSet descriptor_set);

    uint32_t GetVertexCount();
    uint32_t GetTriangleCount();

    const std::vector<Mesh>& GetMeshes();

    const std::string& GetMaterialName();

private:
    std::vector<Mesh> meshes_;
    std::vector<tinyobj::material_t> materials_;
};