#pragma once

#include "Scene/SceneNode.h"
#include "Core/Types.h"
#include <functional>
#include <memory>

namespace RRE
{

class SceneGraph
{
public:
    SceneGraph();
    ~SceneGraph() = default;

    SceneNode* GetRoot() const { return m_root.get(); }

    // Depth-first traversal: visitor(node, worldMatrix)
    using Visitor = std::function<void(SceneNode*, const DirectX::XMMATRIX&)>;
    void Traverse(const Visitor& visitor) const;

    // Sum of all mesh polygon counts
    uint32 GetTotalPolygonCount() const;

private:
    void TraverseNode(SceneNode* node, const DirectX::XMMATRIX& parentWorld,
        const Visitor& visitor) const;
    uint32 CountPolygons(SceneNode* node) const;

    std::unique_ptr<SceneNode> m_root;
};

} // namespace RRE
