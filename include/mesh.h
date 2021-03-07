#pragma once

#include <vector>

#include "common.h"
#include "common_vulkan.h"

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

    vk::Buffer GetVertexBuffer() const;
    vk::Buffer GetIndexBuffer() const;

    uint32_t GetVertexCount() const;
    uint32_t GetTriangleCount() const;

private:
    std::string name_;

    std::vector<int32_t> material_indices_;

    GpuBuffer gpu_vertices_;
    GpuBuffer gpu_indices_;

    uint32_t vertex_count_;
    uint32_t tri_count_;
};