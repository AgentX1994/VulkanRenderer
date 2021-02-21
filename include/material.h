#pragma once

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "tiny_obj_loader.h"

class RendererState;

class Material
{
public:
    Material(RendererState& renderer, tinyobj::material_t material_defintion);

    Material(const Material&) = delete;
    Material(Material&& other);

    Material& operator=(const Material&) = delete;
    Material& operator=(Material&& other);

    ~Material();

    vk::PipelineLayout& GetGraphicsPipelineLayout();
    vk::Pipeline& GetGraphicsPipeline();

private:
    void Cleanup();
    void MoveFrom(Material&& other);

    std::pair<vk::PipelineLayout, vk::Pipeline> CreateGraphicsPipeline(
        RendererState& renderer);
    vk::ShaderModule CreateShaderModule(const std::vector<uint32_t>& code);

    vk::Device& device_;

    vk::PipelineLayout pipeline_layout_;
    vk::Pipeline pipeline_;

    tinyobj::material_t material_;
};