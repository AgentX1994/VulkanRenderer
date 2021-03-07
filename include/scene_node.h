#pragma once

#include "common.h"
#include "common_vulkan.h"

class SceneGraph;
class RenderObject;

class SceneNode
{
public:
    SceneNode(SceneGraph& owner, NonOwningPointer<SceneNode> parent = nullptr,
              glm::vec3 translation = glm::vec3(0),
              glm::quat rotation = glm::quat(), glm::vec3 scale = glm::vec3(1),
              std::vector<NonOwningPointer<SceneNode>> children = {},
              NonOwningPointer<RenderObject> render_object = nullptr);

    void SetParent(NonOwningPointer<SceneNode> new_parent);
    
    void SetTranslation(glm::vec3 translation);
    void SetRotation(glm::quat rotation);
    void SetScale(glm::vec3 scale);

    glm::mat4& GetTransform();

    void SetRenderObject(NonOwningPointer<RenderObject> render_object);
    NonOwningPointer<RenderObject> GetRenderObject();

    NonOwningPointer<SceneNode> CreateChildNode();

private:
    void UpdateCachedTransform();

    SceneGraph& owner_;
    NonOwningPointer<SceneNode> parent_;

    glm::vec3 parent_relative_translation_;
    glm::quat parent_relative_rotation_;
    glm::vec3 parent_relative_scale_;

    std::vector<NonOwningPointer<SceneNode>> children_;

    bool transform_dirty_;
    glm::mat4 cached_transform_;

    NonOwningPointer<RenderObject> render_object_;
};