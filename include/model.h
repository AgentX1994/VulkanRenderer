#pragma once

#include <string>
#include <vector>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "mesh.h"
#include "tiny_obj_loader.h"

class Model
{
public:
    Model(RendererState& renderer,
          std::string path);

    void RecordDrawCommand(vk::CommandBuffer& command_buffer);

    uint32_t GetVertexCount();
    uint32_t GetTriangleCount();

private:
    std::vector<Mesh> meshes_;
    std::vector<tinyobj::material_t> materials_;
};