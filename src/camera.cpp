#include "camera.h"

#include <iostream>

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
    glm::vec3 axis = GetForwardVector();
    Move(axis * amount);
}

void Camera::MoveRight(float amount)
{
    assert(owning_node_);
    glm::vec3 axis = GetRightVector();
    Move(axis * amount);
}

void Camera::MoveUp(float amount)
{
    assert(owning_node_);
    glm::vec3 axis = GetUpVector();
    Move(axis * amount);
}

glm::vec3 Camera::GetForwardVector() const
{
    assert(owning_node_);
    return glm::normalize(owning_node_->RotateVector(Coords::FORWARD));
}

glm::vec3 Camera::GetUpVector() const
{
    assert(owning_node_);
    return glm::normalize(owning_node_->RotateVector(Coords::UP));
}

glm::vec3 Camera::GetRightVector() const
{
    assert(owning_node_);
    return glm::normalize(owning_node_->RotateVector(Coords::RIGHT));
}

glm::vec3 Camera::GetAngles() const { return angles_; }

void Camera::LookAt(glm::vec3 point, glm::vec3 up)
{
    assert(owning_node_);
    owning_node_->SetLookAt(point, up);
    ExtractAngles();
}

void Camera::SetAngles(glm::vec3 angles)
{
    angles_ = angles;
    WrapAngles();
    InjectAngles();
}

void Camera::Rotate(glm::quat rotation)
{
    assert(owning_node_);
    owning_node_->Rotate(rotation);
    ExtractAngles();
}

void Camera::Rotate(glm::vec3 rotation)
{
    angles_ += rotation;
    WrapAngles();
    InjectAngles();
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
    // camera_data.proj[1][1] *= -1;

    camera_data.viewproj = camera_data.proj * camera_data.view;

    return camera_data;
}

void Camera::WrapAngles()
{
    // pitch
    // Pitch is a special case, as it should not wrap
    if (angles_.x < -glm::half_pi<float>()) {
        angles_.x = -glm::half_pi<float>();
    } else if (angles_.x > glm::half_pi<float>()) {
        angles_.x = glm::half_pi<float>();
    }

    // yaw
    if (angles_.y < -glm::pi<float>()) {
        angles_.y += glm::two_pi<float>();
    } else if (angles_.y > glm::pi<float>()) {
        angles_.y -= glm::two_pi<float>();
    }

    // roll
    if (angles_.z < -glm::pi<float>()) {
        angles_.z += glm::two_pi<float>();
    } else if (angles_.z > glm::pi<float>()) {
        angles_.z -= glm::two_pi<float>();
    }
}

void Camera::InjectAngles()
{
    assert(owning_node_);
    auto rot = glm::quat(angles_);
    owning_node_->SetRotation(rot);
}

void Camera::ExtractAngles()
{
    assert(owning_node_);
    auto rot = owning_node_->GetRotation();
    angles_ = glm::eulerAngles(rot);
}