#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "mesh.h"
#include "tiny_obj_loader.h"

class Model
{
public:
    Model(vk::PhysicalDevice& physical_device, vk::Device& device,
          vk::CommandPool& transient_command_pool, vk::Queue& queue,
          std::string path);

    void RecordDrawCommand(vk::CommandBuffer& command_buffer);

private:
    std::vector<Mesh> meshes_;
    std::vector<tinyobj::material_t> materials_;
};