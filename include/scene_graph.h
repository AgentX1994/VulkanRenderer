#pragma once

#include <vector>

#include "common.h"
#include "common_vulkan.h"

#include "scene_node.h"

class SceneGraph
{
public:
    SceneGraph();

    SceneGraph(const SceneGraph&) = delete;
    SceneGraph(SceneGraph&&) = delete;

    SceneGraph& operator=(const SceneGraph&) = delete;
    SceneGraph& operator=(SceneGraph&&) = delete;

    ~SceneGraph();

    NonOwningPointer<SceneNode> GetRoot();

    NonOwningPointer<SceneNode> CreateNewSceneNode();

private:
    NonOwningPointer<SceneNode> root_;
    std::vector<std::unique_ptr<SceneNode>> nodes_;
};