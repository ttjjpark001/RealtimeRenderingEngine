#include "Scene/SceneGraph.h"
#include "Renderer/Mesh.h"

namespace RRE
{

SceneGraph::SceneGraph()
    : m_root(std::make_unique<SceneNode>())
{
}

void SceneGraph::SetRoot(std::unique_ptr<SceneNode> newRoot)
{
    m_root = std::move(newRoot);
}

void SceneGraph::Traverse(const Visitor& visitor) const
{
    if (m_root)
    {
        DirectX::XMMATRIX identity = DirectX::XMMatrixIdentity();
        TraverseNode(m_root.get(), identity, visitor);
    }
}

void SceneGraph::TraverseNode(SceneNode* node, const DirectX::XMMATRIX& parentWorld,
    const Visitor& visitor) const
{
    DirectX::XMMATRIX localMatrix = node->GetTransform().GetLocalMatrix();
    DirectX::XMMATRIX worldMatrix = localMatrix * parentWorld;

    visitor(node, worldMatrix);

    for (auto& child : node->GetChildren())
    {
        TraverseNode(child.get(), worldMatrix, visitor);
    }
}

SceneNode* SceneGraph::FindNodeByName(const std::string& name) const
{
    return FindNodeInSubtree(m_root.get(), name);
}

SceneNode* SceneGraph::FindNodeInSubtree(SceneNode* node, const std::string& name) const
{
    if (!node) return nullptr;
    if (node->GetName() == name) return node;
    for (auto& child : node->GetChildren())
    {
        SceneNode* found = FindNodeInSubtree(child.get(), name);
        if (found) return found;
    }
    return nullptr;
}

uint32 SceneGraph::GetTotalPolygonCount() const
{
    if (!m_root)
        return 0;
    return CountPolygons(m_root.get());
}

uint32 SceneGraph::CountPolygons(SceneNode* node) const
{
    uint32 count = 0;

    if (node->GetMesh())
    {
        count += node->GetMesh()->GetPolygonCount();
    }

    for (auto& child : node->GetChildren())
    {
        count += CountPolygons(child.get());
    }

    return count;
}

} // namespace RRE
