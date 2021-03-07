#pragma once

#include "common.h"
#include "common_vulkan.h"

struct GpuCameraData {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::mat4 viewproj;
};

class Camera {
public:
    Camera();
};