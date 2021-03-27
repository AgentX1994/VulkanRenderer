#include "camera.h"

#include "scene_node.h"

Camera::Camera(float fov, float aspect_ratio, float near_z, float far_z)
    : fov_(fov),
      aspect_ratio_(aspect_ratio_),
      near_z_(near_z),
      far_z_(far_z),
      owning_node_(nullptr)
{}

void Camera::SetFov(float fov) { fov_ = fov; }

float Camera::GetFov() const { return fov_; }

void Camera::SetAspectRatio(float aspect_ratio)
{
    aspect_ratio_ = aspect_ratio;
}

float Camera::GetAspectRatio() const { return aspect_ratio_; }

void Camera::SetNearZ(float near_z) { near_z_ = near_z; }

float Camera::GetNearZ() const { return near_z_; }

void Camera::SetFarZ(float far_z) { far_z_ = far_z; }

float Camera::GetFarZ() const { return far_z_; }

void Camera::SetNode(NonOwningPointer<SceneNode> node) { owning_node_ = node; }

NonOwningPointer<SceneNode> Camera::GetNode() const { return owning_node_; }

void Camera::SetPosition(glm::vec3 pos)
{
    assert(owning_node_);
    owning_node_->SetTranslation(pos);
}

void Camera::Move(glm::vec3 translation)
{
    assert(owning_node_);
    owning_node_->Translate(translation);
}

void Camera::MoveForward(float amount)
{
    assert(owning_node_);
    glm::vec3 axis = glm::normalize(owning_node_->RotateVector({0.0, 0.0, -1.0}));
    Move(axis*amount);
}

void Camera::MoveRight(float amount)
{
    assert(owning_node_);
    glm::vec3 axis = glm::normalize(owning_node_->RotateVector({1.0, 0.0, 0.0}));
    Move(axis*amount);
}

void Camera::MoveUp(float amount)
{
    assert(owning_node_);
    glm::vec3 axis = glm::normalize(owning_node_->RotateVector({0.0, 1.0, 0.0}));
    Move(axis*amount);
}

glm::vec3 Camera::GetUpVector() const {
    assert(owning_node_);
    return glm::normalize(owning_node_->RotateVector({0.0, 0.0, 1.0}));
}
glm::vec3 Camera::GetRightVector() const {
    assert(owning_node_);
    return glm::normalize(owning_node_->RotateVector({1.0, 0.0, 0.0}));
}

void Camera::LookAt(glm::vec3 point, glm::vec3 up)
{
    assert(owning_node_);
    owning_node_->SetLookAt(point, up);
}

void Camera::Rotate(glm::quat rotation)
{
    assert(owning_node_);
    owning_node_->Rotate(rotation);
}

GpuCameraData Camera::GetCameraData() const
{
    GpuCameraData camera_data;
    if (owning_node_ != nullptr) {
        // view is the inverse of the camera transform
        camera_data.view = glm::inverse(owning_node_->GetTransform());
    } else {
        camera_data.view = glm::mat4(1);
    }

    camera_data.proj =
        glm::perspective(glm::radians(fov_), aspect_ratio_, near_z_, far_z_);
    // compensate for incorect y coordinate in clipping space (OpenGL has it
    // flipped compared to Vulkan)
    camera_data.proj[1][1] *= -1;

    camera_data.viewproj = camera_data.proj * camera_data.view;

    return camera_data;
}