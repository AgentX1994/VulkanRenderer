#pragma once

#include "common.h"
#include "common_vulkan.h"

#include "tiny_obj_loader.h"

class RendererState;
class Texture;

class Material
{
public:
    Material(RendererState& renderer, tinyobj::material_t material_defintion);

    Material(const Material&) = delete;
    Material(Material&& other);

    Material& operator=(const Material&) = delete;
    Material& operator=(Material&& other);

    ~Material();

    void RecreatePipeline(RendererState& renderer);

    vk::PipelineLayout& GetGraphicsPipelineLayout();
    vk::Pipeline& GetGraphicsPipeline();

    NonOwningPointer<Texture> GetTexture();
    vk::DescriptorSet& GetDescriptorSet();

private:
    void CleanupPipeline();
    void Cleanup();
    void MoveFrom(Material&& other);

    std::pair<vk::PipelineLayout, vk::Pipeline> CreateGraphicsPipeline(
        RendererState& renderer);
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>& code);

    void CreateSampler(RendererState& renderer);
    void CreateDescriptorSet(RendererState& renderer);

    vk::Device& device_;

    vk::PipelineLayout pipeline_layout_;
    vk::Pipeline pipeline_;

    tinyobj::material_t material_;
    vk::DescriptorSet material_descriptor_set_;
    NonOwningPointer<Texture> texture_;
    vk::Sampler sampler_;
};