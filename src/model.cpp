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
        static_assert(std::is_move_constructible_v<Mesh>);
        Mesh mesh(renderer, attrib, shape);
        meshes_.emplace_back(std::move(mesh));
        // meshes_.emplace_back(physical_device, device, transient_command_pool,
        // queue,
        //           attrib, shape);
    }

    for (auto mat : materials_) {
        renderer.GetMaterialCache().LoadMaterial(renderer, mat.name, mat);
    }
}

void Model::RecordDrawCommand(
    RendererState& renderer, vk::CommandBuffer& command_buffer,
    /* temporary */ vk::DescriptorSet descriptor_set)
{
    Material* material =
        renderer.GetMaterialCache().GetMaterialByName(materials_[0].name);
    if (material != nullptr) {
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    material->GetGraphicsPipeline());

        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                          material->GetGraphicsPipelineLayout(),
                                          0, descriptor_set, {});
    }
    for (auto& mesh : meshes_) {
        mesh.RecordDrawCommand(command_buffer);
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