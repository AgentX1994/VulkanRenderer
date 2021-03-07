#include "scene_graph.h"

#include <utility>

SceneGraph::SceneGraph()
{
    // this allocates a new node with identity transform, nullptr parent, and no
    // children
    nodes_.push_back(std::make_unique<SceneNode>(*this));
    root_ = nodes_[0].get();
}

SceneGraph::~SceneGraph() {}
NonOwningPointer<SceneNode> SceneGraph::GetRoot() { return root_; }

NonOwningPointer<SceneNode> SceneGraph::CreateNewSceneNode()
{
    nodes_.push_back(std::make_unique<SceneNode>(*this));
    return nodes_.back().get();
}