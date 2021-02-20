#pragma once

#include <vector>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "gpu_buffer.h"
#include "tiny_obj_loader.h"
#include "vertex.h"

class Mesh
{
public:
    Mesh(RendererState& renderer, const tinyobj::attrib_t attribs,
         const tinyobj::shape_t& shape);
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&& mesh);

    void RecordDrawCommand(vk::CommandBuffer& command_buffer);

    uint32_t GetVertexCount();
    uint32_t GetTriangleCount();

private:
    std::string name_;

    std::vector<int32_t> material_indices_;

    GpuBuffer gpu_vertices_;
    GpuBuffer gpu_indices_;

    uint32_t vertex_count_;
    uint32_t tri_count_;
};