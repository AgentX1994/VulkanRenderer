#include "vertex.h"

#include <glm/gtx/hash.hpp>

vk::VertexInputBindingDescription Vertex::GetBindingDescription()
{
    return vk::VertexInputBindingDescription(0, sizeof(Vertex),
                                             vk::VertexInputRate::eVertex);
}

std::array<vk::VertexInputAttributeDescription, 3>
Vertex::GetAttributeDescriptions()
{
    return {vk::VertexInputAttributeDescription(
                0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(
                1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,
                                                offsetof(Vertex, tex_coord))};
}

bool Vertex::operator==(const Vertex& other) const
{
    return pos == other.pos && color == other.color &&
           tex_coord == other.tex_coord;
}

namespace std {
size_t hash<Vertex>::operator()(const Vertex& vertex) const
{
    return ((hash<glm::vec3>()(vertex.pos) ^
             (hash<glm::vec3>()(vertex.color) << 1)) >>
            1) ^
           (hash<glm::vec2>()(vertex.tex_coord) << 1);
}
}  // namespace std