#include "render_object.h"

#include "gpu_buffer.h"
#include "model.h"
#include "renderer_state.h"
#include "scene_node.h"

RenderObject::RenderObject(RendererState& renderer)
    : device_(renderer.GetDevice()),
      owning_node_(nullptr),
      model_(nullptr),
      object_properties_buffer_(renderer, sizeof(GpuObjectData),
                                vk::BufferUsageFlagBits::eUniformBuffer,
                                vk::MemoryPropertyFlagBits::eHostVisible |
                                    vk::MemoryPropertyFlagBits::eHostCoherent)
{
    CreateDescriptorSet(renderer);
    UpdateTransform();
}

void RenderObject::SetNode(NonOwningPointer<SceneNode> owning_node)
{
    owning_node_ = owning_node;
    UpdateTransform();
}

void RenderObject::SetModel(NonOwningPointer<Model> model) { model_ = model; }

NonOwningPointer<SceneNode> RenderObject::GetNode() { return owning_node_; }

NonOwningPointer<Model> RenderObject::GetModel() { return model_; }

vk::DescriptorSet& RenderObject::GetDescriptorSet() { return object_set_; }

void RenderObject::UpdateTransform()
{
    GpuObjectData object_properties;
    if (!owning_node_) {
        object_properties.transform = glm::mat4(1);
    } else {
        object_properties.transform = owning_node_->GetTransform();
    }

    void* data = device_.mapMemory(object_properties_buffer_.GetMemory(), 0,
                                   sizeof(GpuObjectData));
    memcpy(data, &object_properties, sizeof(GpuObjectData));
    device_.unmapMemory(object_properties_buffer_.GetMemory());
}

void RenderObject::CreateDescriptorSet(RendererState& renderer)
{
    std::vector<vk::DescriptorSetLayout> layouts(
        1, renderer.GetObjectDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo alloc_info(renderer.GetDescriptorPool(),
                                             layouts);
    object_set_ = renderer.GetDevice().allocateDescriptorSets(alloc_info)[0];

    vk::DescriptorBufferInfo buffer_info(object_properties_buffer_.GetBuffer(),
                                         0, sizeof(GpuObjectData));

    vk::WriteDescriptorSet descriptor_write(
        object_set_, 0, 0, vk::DescriptorType::eUniformBuffer, {}, buffer_info);

    device_.updateDescriptorSets(descriptor_write, {});
}