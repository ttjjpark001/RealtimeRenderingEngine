#include "Scene/SceneNode.h"

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

} // namespace RRE
