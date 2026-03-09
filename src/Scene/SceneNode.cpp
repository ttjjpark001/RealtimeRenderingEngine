#include "Scene/SceneNode.h"
#include "Renderer/Mesh.h"

namespace RRE
{

SceneNode* SceneNode::AddChild(std::unique_ptr<SceneNode> child)
{
    child->m_parent = this;
    SceneNode* rawPtr = child.get();
    m_children.push_back(std::move(child));
    return rawPtr;
}

std::unique_ptr<SceneNode> SceneNode::RemoveChild(SceneNode* child)
{
    for (auto it = m_children.begin(); it != m_children.end(); ++it)
    {
        if (it->get() == child)
        {
            child->m_parent = nullptr;
            std::unique_ptr<SceneNode> removed = std::move(*it);
            m_children.erase(it);
            return removed;
        }
    }
    return nullptr;
}

DirectX::XMMATRIX SceneNode::GetWorldMatrix() const
{
    DirectX::XMMATRIX localMatrix = m_localTransform.GetLocalMatrix();

    if (m_parent)
    {
        return localMatrix * m_parent->GetWorldMatrix();
    }

    return localMatrix;
}

DirectX::BoundingBox SceneNode::GetWorldAABB() const
{
    if (m_aabbDirty)
    {
        if (m_mesh && !m_mesh->vertices.empty())
        {
            m_mesh->aabb.Transform(m_worldAABB, GetWorldMatrix());
            // Ensure non-degenerate AABB to prevent frustum cull false negatives on flat meshes.
            // 0.5 (= 1.0 unit total height) gives a generous margin for thin floor/wall geometry.
            const float kMinExtent = 0.5f;
            if (m_worldAABB.Extents.x < kMinExtent) m_worldAABB.Extents.x = kMinExtent;
            if (m_worldAABB.Extents.y < kMinExtent) m_worldAABB.Extents.y = kMinExtent;
            if (m_worldAABB.Extents.z < kMinExtent) m_worldAABB.Extents.z = kMinExtent;
        }
        else
        {
            m_worldAABB = DirectX::BoundingBox({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        }
        m_aabbDirty = false;
    }
    return m_worldAABB;
}

} // namespace RRE
