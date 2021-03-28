#include <gmock/gmock-matchers.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest.h>

#include "scene_graph.h"
#include "scene_node.h"
#include "utils.h"

using testing::FloatEq;

TEST(Math, RotationBetweenVectors)
{
    auto v1 = glm::vec3(1.0, 0.0, 0.0);
    auto v2 = glm::vec3(0.0, 1.0, 0.0);

    auto rot = RotationBetweenVectors(v1, v2);
    auto angles = glm::eulerAngles(rot);
    // angle should be 90.0 degrees (pi/2 radians) around z axis, 0 elsewhere
    ASSERT_THAT(angles.x, FloatEq(0.0f));
    ASSERT_THAT(angles.y, FloatEq(0.0f));
    ASSERT_THAT(angles.z, FloatEq(glm::half_pi<float>()));
}

TEST(Math, LookAtNoRotation)
{
    auto rot = QuaternionLookAt({0.0, 0.0, 1.0}, {0.0, 0.0, 0.0});
    auto angles = glm::eulerAngles(rot);
    // 0.0, 0.0, 1.0 -> 0.0, 0.0, 0.0f = 0.0, 0.0, -1.0
    // This should mean 0 rotation
    ASSERT_THAT(angles.x, FloatEq(0));
    ASSERT_THAT(angles.y, FloatEq(0));
    ASSERT_THAT(angles.z, FloatEq(0));
}

TEST(Math, LookAt90Rotation)
{
    // This needs the up vector to not be +y or else the math explodes
    auto rot = QuaternionLookAt({0.0, -1.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0});
    auto angles = glm::eulerAngles(rot);
    // 0.0, -1.0, 0.0 -> 0.0, 0.0, 0.0f = 0.0, 1.0, 0.0
    // This should mean 90 degree (pi/2) pitch (x) rotation
    ASSERT_THAT(angles.x, FloatEq(glm::half_pi<float>()));
    ASSERT_THAT(angles.y, FloatEq(0));
    ASSERT_THAT(angles.z, FloatEq(0));
}