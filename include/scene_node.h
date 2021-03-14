#pragma once

#include "common.h"
#include "common_vulkan.h"

class SceneGraph;
class RenderObject;
class Camera;

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
    glm::vec3 GetTranslation() const;

    void SetRotation(glm::quat rotation);
    glm::quat GetRotation() const;

    void SetScale(glm::vec3 scale);
    glm::vec3 GetScale() const;

    void SetLookAt(glm::vec3 point, glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 GetTransform() const;

    void SetRenderObject(NonOwningPointer<RenderObject> render_object);
    NonOwningPointer<RenderObject> GetRenderObject() const;

    void SetCamera(NonOwningPointer<Camera> camera);
    NonOwningPointer<Camera> GetCamera() const;

    NonOwningPointer<SceneNode> CreateChildNode();

private:
    void UpdateCachedTransform() const;

    SceneGraph& owner_;
    NonOwningPointer<SceneNode> parent_;

    glm::vec3 parent_relative_translation_;
    glm::quat parent_relative_rotation_;
    glm::vec3 parent_relative_scale_;

    std::vector<NonOwningPointer<SceneNode>> children_;

    mutable bool transform_dirty_;
    mutable glm::mat4 cached_transform_;

    NonOwningPointer<RenderObject> render_object_;
    NonOwningPointer<Camera> camera_;
};