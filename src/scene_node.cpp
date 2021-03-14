#include "scene_node.h"

#include "camera.h"
#include "common.h"
#include "common_vulkan.h"
#include "render_object.h"
#include "scene_graph.h"

SceneNode::SceneNode(SceneGraph& owner, NonOwningPointer<SceneNode> parent,
                     glm::vec3 translation, glm::quat rotation, glm::vec3 scale,
                     std::vector<NonOwningPointer<SceneNode>> children,
                     NonOwningPointer<RenderObject> render_object)
    : owner_(owner),
      parent_(parent),
      parent_relative_translation_(translation),
      parent_relative_rotation_(rotation),
      parent_relative_scale_(scale),
      children_(std::move(children)),
      transform_dirty_(true),
      cached_transform_(glm::mat4(1)),
      render_object_(render_object)
{
    UpdateCachedTransform();
}

void SceneNode::SetParent(NonOwningPointer<SceneNode> new_parent)
{
    parent_ = new_parent;
    transform_dirty_ = true;
}

void SceneNode::SetTranslation(glm::vec3 translation)
{
    parent_relative_translation_ = translation;
    transform_dirty_ = true;
}

glm::vec3 SceneNode::GetTranslation() const
{
    return parent_relative_translation_;
}

void SceneNode::SetRotation(glm::quat rotation)
{
    parent_relative_rotation_ = rotation;
    transform_dirty_ = true;
}

glm::quat SceneNode::GetRotation() const { return parent_relative_rotation_; }

void SceneNode::SetScale(glm::vec3 scale)
{
    parent_relative_scale_ = scale;
    transform_dirty_ = true;
}

glm::vec3 SceneNode::GetScale() const { return parent_relative_scale_; }

void SceneNode::SetLookAt(glm::vec3 point, glm::vec3 up)
{
    glm::mat4 transform = glm::lookAt(parent_relative_translation_, point, up);
    parent_relative_rotation_ = glm::conjugate(glm::toQuat(transform));
    transform_dirty_ = true;
}

glm::mat4 SceneNode::GetTransform() const
{
    if (transform_dirty_) {
        UpdateCachedTransform();
    }
    return cached_transform_;
}

void SceneNode::SetRenderObject(NonOwningPointer<RenderObject> render_object)
{
    render_object_ = render_object;
    if (render_object_) {
        render_object_->SetNode(this);
    }
}

NonOwningPointer<RenderObject> SceneNode::GetRenderObject() const
{
    return render_object_;
}

void SceneNode::SetCamera(NonOwningPointer<Camera> camera)
{
    camera_ = camera;
    if (camera_) {
        camera_->SetNode(this);
    }
}

NonOwningPointer<Camera> SceneNode::GetCamera() const { return camera_; }

NonOwningPointer<SceneNode> SceneNode::CreateChildNode()
{
    auto new_node = owner_.CreateNewSceneNode();
    new_node->SetParent(this);
    children_.push_back(new_node);
    return new_node;
}

void SceneNode::UpdateCachedTransform() const
{
    glm::mat4 parent_relative_transform = glm::mat4(1);
    // construct the transform
    parent_relative_transform =
        glm::translate(parent_relative_transform, parent_relative_translation_);
    parent_relative_transform *= glm::toMat4(parent_relative_rotation_);
    parent_relative_transform =
        glm::scale(parent_relative_transform, parent_relative_scale_);

    if (!parent_) {
        cached_transform_ = parent_relative_transform;
    } else {
        auto parent_transform = parent_->GetTransform();
        cached_transform_ = parent_transform * parent_relative_transform;
    }

    transform_dirty_ = false;

    for (auto child : children_) {
        child->UpdateCachedTransform();
    }

    if (render_object_) {
        render_object_->UpdateTransform();
    }
}