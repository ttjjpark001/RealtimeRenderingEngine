#pragma once

#include "Scene/Transform.h"
#include <DirectXMath.h>
#include <vector>
#include <memory>

namespace RRE
{

class Mesh;

class SceneNode
{
public:
    SceneNode() = default;
    ~SceneNode() = default;

    // Tree operations
    SceneNode* AddChild(std::unique_ptr<SceneNode> child);
    std::unique_ptr<SceneNode> RemoveChild(SceneNode* child);
    SceneNode* GetParent() const { return m_parent; }
    const std::vector<std::unique_ptr<SceneNode>>& GetChildren() const { return m_children; }

    // World matrix: parent's world matrix * local matrix (recursive)
    DirectX::XMMATRIX GetWorldMatrix() const;

    // Transform
    Transform& GetTransform() { return m_localTransform; }
    const Transform& GetTransform() const { return m_localTransform; }

    // Mesh (nullable)
    void SetMesh(Mesh* mesh) { m_mesh = mesh; }
    Mesh* GetMesh() const { return m_mesh; }

private:
    Transform m_localTransform;
    Mesh* m_mesh = nullptr;
    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;
};

} // namespace RRE
