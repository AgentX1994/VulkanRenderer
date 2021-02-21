#include "material.h"

#include "renderer_state.h"
#include "swapchain.h"
#include "utils.h"
#include "vertex.h"

Material::Material(RendererState& renderer,
                   tinyobj::material_t material_defintion)
    : device_(renderer.GetDevice()), material_(material_defintion)
{
    std::tie(pipeline_layout_, pipeline_) = CreateGraphicsPipeline(renderer);
}

Material::Material(Material&& other) : device_(other.device_)
{
    MoveFrom(std::move(other));
}

Material& Material::operator=(Material&& other)
{
    Cleanup();
    MoveFrom(std::move(other));
    return *this;
}

Material::~Material() { Cleanup(); }

vk::PipelineLayout& Material::GetGraphicsPipelineLayout()
{
    return pipeline_layout_;
}

vk::Pipeline& Material::GetGraphicsPipeline()
{
    return pipeline_;
}

void Material::Cleanup()
{
    if (pipeline_layout_) {
        device_.destroyPipelineLayout(pipeline_layout_);
    }
    if (pipeline_) {
        device_.destroyPipeline(pipeline_);
    }
}

void Material::MoveFrom(Material&& other)
{
    device_ = other.device_;
    pipeline_layout_ = std::move(other.pipeline_layout_);
    other.pipeline_layout_ = (VkPipelineLayout)VK_NULL_HANDLE;
    pipeline_ = std::move(other.pipeline_);
    other.pipeline_ = (VkPipeline)VK_NULL_HANDLE;
    material_ = std::move(other.material_);
}

std::pair<vk::PipelineLayout, vk::Pipeline> Material::CreateGraphicsPipeline(
    RendererState& renderer)
{
    auto vert_shader_bin =
        CompileShader("shaders/shader.vert", shaderc_glsl_vertex_shader);
    auto frag_shader_bin =
        CompileShader("shaders/shader.frag", shaderc_glsl_fragment_shader);

    auto vert_shader_module = CreateShaderModule(vert_shader_bin);
    auto frag_shader_module = CreateShaderModule(frag_shader_bin);

    vk::PipelineShaderStageCreateInfo vert_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex,
        vert_shader_module, "main");
    vk::PipelineShaderStageCreateInfo frag_shader_stage_info(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment, frag_shader_module, "main");

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages = {
        vert_shader_stage_info, frag_shader_stage_info};

    auto bindingDescription = Vertex::GetBindingDescription();
    auto attributeDescriptions = Vertex::GetAttributeDescriptions();
    vk::PipelineVertexInputStateCreateInfo vertex_input_info(
        vk::PipelineVertexInputStateCreateFlags(),
        bindingDescription,    // Vertex Binding Descriptions
        attributeDescriptions  // Vertex attribute descriptions
    );

    vk::PipelineInputAssemblyStateCreateInfo input_assembly(
        vk::PipelineInputAssemblyStateCreateFlags(),
        vk::PrimitiveTopology::eTriangleList, VK_FALSE);

    auto extent = renderer.GetSwapchain().GetExtent();
    vk::Viewport viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height,
                          0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, extent);

    vk::PipelineViewportStateCreateInfo viewport_state(
        vk::PipelineViewportStateCreateFlags(), viewport, scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizer(
        vk::PipelineRasterizationStateCreateFlags(), VK_FALSE, VK_FALSE,
        vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
        vk::FrontFace::eCounterClockwise, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling(
        vk::PipelineMultisampleStateCreateFlags(), renderer.GetMaxSampleCount(),
        VK_TRUE, 0.2f, nullptr, VK_FALSE, VK_FALSE);

    vk::PipelineColorBlendAttachmentState color_blend_attachment(
        VK_TRUE, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo color_blending(
        vk::PipelineColorBlendStateCreateFlags(), VK_FALSE, vk::LogicOp::eCopy,
        color_blend_attachment, {0.0f, 0.0f, 0.0f, 0.0f});

    std::vector<vk::DynamicState> dynamic_states = {
        // vk::DynamicState::eViewport,
        // vk::DynamicState::eLineWidth
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state(
        vk::PipelineDynamicStateCreateFlags(), dynamic_states);

    vk::PipelineLayoutCreateInfo pipeline_layout_info(
        vk::PipelineLayoutCreateFlags(),
        renderer.GetDescriptorSetLayout(),  // Set layouts
        {}                                  // Push constant ranges
    );

    auto pipeline_layout =
        renderer.GetDevice().createPipelineLayout(pipeline_layout_info);

    vk::PipelineDepthStencilStateCreateInfo depth_stencil(
        vk::PipelineDepthStencilStateCreateFlags(), VK_TRUE, VK_TRUE,
        vk::CompareOp::eLess, VK_FALSE, VK_FALSE, {}, {});

    vk::GraphicsPipelineCreateInfo pipeline_info(
        vk::PipelineCreateFlags(), shader_stages, &vertex_input_info,
        &input_assembly, {}, &viewport_state, &rasterizer, &multisampling,
        &depth_stencil, &color_blending, &dynamic_state, pipeline_layout,
        renderer.GetRenderPass(), 0, {}, -1);

    vk::Pipeline pipeline;
    auto result =
        renderer.GetDevice().createGraphicsPipeline({}, pipeline_info);
    switch (result.result) {
        case vk::Result::eSuccess:
            pipeline = result.value;
            break;
        case vk::Result::ePipelineCompileRequiredEXT:
            throw std::runtime_error("Pipeline compile required (WTF?)");
    }

    renderer.GetDevice().destroyShaderModule(frag_shader_module);
    renderer.GetDevice().destroyShaderModule(vert_shader_module);

    return {pipeline_layout, pipeline};
}

vk::ShaderModule Material::CreateShaderModule(const std::vector<uint32_t>& code)
{
    vk::ShaderModuleCreateInfo create_info(vk::ShaderModuleCreateFlagBits(),
                                           code);

    return device_.createShaderModule(create_info);
}