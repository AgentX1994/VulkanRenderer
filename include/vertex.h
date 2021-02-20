#pragma once

#include <array>
#include <glm/glm.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 tex_coord;

    static vk::VertexInputBindingDescription GetBindingDescription();
    static std::array<vk::VertexInputAttributeDescription, 3>
    GetAttributeDescriptions();
    bool operator==(const Vertex& other) const;
};

namespace std {
template <>
struct hash<Vertex>
{
    size_t operator()(const Vertex& vertex) const;
};
}  // namespace std