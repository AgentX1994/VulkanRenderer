#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "tiny_obj_loader.h"
#include "vertex.h"
#include "gpu_buffer.h"

class Mesh
{
public:
    Mesh(vk::PhysicalDevice& physical_device, vk::Device& device,
         vk::CommandPool& transient_command_pool, vk::Queue& queue,
         const tinyobj::attrib_t attribs, const tinyobj::shape_t& shape);
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&& mesh);

    void RecordDrawCommand(vk::CommandBuffer& command_buffer);

private:
    std::string name_;

    std::vector<int32_t> material_indices_;

    GpuBuffer gpu_vertices_;
    GpuBuffer gpu_indices_;

    uint32_t vertex_count_;
    uint32_t tri_count_;
};