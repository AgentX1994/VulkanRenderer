#include "mesh.h"

#include <unordered_map>

Mesh::Mesh(RendererState& renderer, const tinyobj::attrib_t attribs,
           const tinyobj::shape_t& shape)
    : gpu_vertices_(renderer.GetDevice()),
      gpu_indices_(renderer.GetDevice()),
      name_(shape.name),
      material_indices_(shape.mesh.material_ids)
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> unique_vertices;

    for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        vertex.pos = {attribs.vertices[3 * index.vertex_index + 0],
                      attribs.vertices[3 * index.vertex_index + 1],
                      attribs.vertices[3 * index.vertex_index + 2]};

        vertex.tex_coord = {
            attribs.texcoords[2 * index.texcoord_index + 0],
            // the loader loads top to bottom, but Vulkan is bottom to top
            1.0f - attribs.texcoords[2 * index.texcoord_index + 1]};

        vertex.color = {attribs.colors[3 * index.vertex_index + 0],
                        attribs.colors[3 * index.vertex_index + 1],
                        attribs.colors[3 * index.vertex_index + 2]};

        if (unique_vertices.count(vertex) == 0) {
            unique_vertices[vertex] = static_cast<uint32_t>(vertices.size());
            vertices.push_back(vertex);
        }

        indices.push_back(unique_vertices[vertex]);
    }

    vertex_count_ = vertices.size();
    tri_count_ = indices.size() / 3;

    gpu_vertices_.SetData(renderer, vertices,
                          vk::BufferUsageFlagBits::eTransferDst |
                              vk::BufferUsageFlagBits::eVertexBuffer,
                          vk::MemoryPropertyFlagBits::eDeviceLocal);
    gpu_indices_.SetData(renderer, indices,
                         vk::BufferUsageFlagBits::eTransferDst |
                             vk::BufferUsageFlagBits::eIndexBuffer,
                         vk::MemoryPropertyFlagBits::eDeviceLocal);
}

Mesh::Mesh(Mesh&& other)
    : name_(std::move(other.name_)),
      gpu_vertices_(std::move(other.gpu_vertices_)),
      gpu_indices_(std::move(other.gpu_indices_)),
      vertex_count_(other.vertex_count_),
      tri_count_(other.tri_count_)
{}

vk::Buffer Mesh::GetVertexBuffer() const { return gpu_vertices_.GetBuffer(); }

vk::Buffer Mesh::GetIndexBuffer() const { return gpu_indices_.GetBuffer(); }

uint32_t Mesh::GetVertexCount() const { return vertex_count_; }

uint32_t Mesh::GetTriangleCount() const { return tri_count_; }