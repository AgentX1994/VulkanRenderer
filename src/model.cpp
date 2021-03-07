#include "model.h"

#include <iostream>
#include <type_traits>

Model::Model(RendererState& renderer, std::string path)
{
    tinyobj::ObjReaderConfig config;
    config.mtl_search_path = "./materials";
    config.triangulate = true;
    config.vertex_color = true;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path, config)) {
        if (!reader.Error().empty()) {
            std::cerr << reader.Error() << '\n';
        }
        exit(1);
    }

    if (!reader.Warning().empty()) {
        std::cout << reader.Warning() << '\n';
    }

    auto& shapes = reader.GetShapes();
    auto& attrib = reader.GetAttrib();
    materials_ = reader.GetMaterials();

    for (const auto& shape : shapes) {
        meshes_.emplace_back(renderer, attrib, shape);
    }

    for (auto mat : materials_) {
        renderer.GetMaterialCache().LoadMaterial(renderer, mat.name, mat);
    }
}

uint32_t Model::GetVertexCount()
{
    uint32_t count = 0;
    for (auto& mesh : meshes_) {
        count += mesh.GetVertexCount();
    }

    return count;
}

uint32_t Model::GetTriangleCount()
{
    uint32_t count = 0;
    for (auto& mesh : meshes_) {
        count += mesh.GetTriangleCount();
    }

    return count;
}

const std::vector<Mesh>& Model::GetMeshes()
{
    return meshes_;
}

const std::string& Model::GetMaterialName()
{
    return materials_[0].name;
}