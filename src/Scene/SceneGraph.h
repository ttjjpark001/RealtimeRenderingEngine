#pragma once

#include "Scene/SceneNode.h"
#include "Core/Types.h"
#include <functional>
#include <memory>
#include <string>

namespace RRE
{

class SceneGraph
{
public:
    SceneGraph();
    ~SceneGraph() = default;

    SceneNode* GetRoot() const { return m_root.get(); }

    // Replace root node (for scene loading)
    void SetRoot(std::unique_ptr<SceneNode> newRoot);

    // Depth-first traversal: visitor(node, worldMatrix)
    using Visitor = std::function<void(SceneNode*, const DirectX::XMMATRIX&)>;
    void Traverse(const Visitor& visitor) const;

    // Find a node by name (depth-first); returns nullptr if not found
    SceneNode* FindNodeByName(const std::string& name) const;

    // Sum of all mesh polygon counts
    uint32 GetTotalPolygonCount() const;

private:
    void TraverseNode(SceneNode* node, const DirectX::XMMATRIX& parentWorld,
        const Visitor& visitor) const;
    SceneNode* FindNodeInSubtree(SceneNode* node, const std::string& name) const;
    uint32 CountPolygons(SceneNode* node) const;

    std::unique_ptr<SceneNode> m_root;
};

} // namespace RRE
