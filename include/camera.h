#pragma once

#include "common.h"
#include "common_glm.h"
#include "common_vulkan.h"

class SceneNode;

struct GpuCameraData
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewproj;
};

class Camera
{
public:
    Camera(float fov = 45.0f, float aspect_ratio = 16.0 / 9.0,
           float near_z = 0.1f, float far_z = 10.0f);

    void SetFov(float fov);
    float GetFov() const;

    void SetAspectRatio(float aspect_ratio);
    float GetAspectRatio() const;

    void SetNearZ(float near_z);
    float GetNearZ() const;

    void SetFarZ(float far_z);
    float GetFarZ() const;

    void SetNode(NonOwningPointer<SceneNode> node);
    NonOwningPointer<SceneNode> GetNode() const;

    void SetPosition(glm::vec3 pos);
    void Move(glm::vec3 translation);

    void MoveForward(float amount);
    void MoveRight(float amount);
    void MoveUp(float amount);

    glm::vec3 GetForwardVector() const;
    glm::vec3 GetUpVector() const;
    glm::vec3 GetRightVector() const;

    void SetAngles(glm::vec3 angles);
    glm::vec3 GetAngles() const;

    void LookAt(glm::vec3 point, glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f));
    void Rotate(glm::quat rotation);
    void Rotate(glm::vec3 angles);

    GpuCameraData GetCameraData() const;

private:
    float fov_;
    float aspect_ratio_;
    float near_z_;
    float far_z_;

    void WrapAngles();
    void InjectAngles();
    void ExtractAngles();

    glm::vec3 angles_; // x = x angle, y = y angle, z = z angle

    NonOwningPointer<SceneNode> owning_node_;
};