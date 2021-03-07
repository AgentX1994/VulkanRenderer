#pragma once

#include "common.h"
#include "common_vulkan.h"
#include "gpu_buffer.h"

class RendererState;
class SceneNode;
class Model;

struct GpuObjectData
{
    alignas(16) glm::mat4 transform;
};

class RenderObject
{
public:
    RenderObject(RendererState& renderer);

    void SetNode(NonOwningPointer<SceneNode> owning_node);
    void SetModel(NonOwningPointer<Model> model);

    NonOwningPointer<SceneNode> GetNode();
    NonOwningPointer<Model> GetModel();

    vk::DescriptorSet& GetDescriptorSet();

    void UpdateTransform();

private:
    void CreateDescriptorSet(RendererState& renderer);

    vk::Device& device_;

    NonOwningPointer<SceneNode> owning_node_;
    NonOwningPointer<Model> model_;

    vk::DescriptorSet object_set_;
    GpuBuffer object_properties_buffer_;
};