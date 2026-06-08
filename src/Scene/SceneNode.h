#pragma once

#include "Scene/Transform.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <vector>
#include <memory>
#include <string>

namespace RRE
{

class Mesh;
class Material;

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

    // Node name (matches aiNode::mName for animation targeting)
    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const    { return m_name; }

    // Transform (mutable access marks AABB dirty)
    Transform& GetTransform() { m_aabbDirty = true; return m_localTransform; }
    const Transform& GetTransform() const { return m_localTransform; }

    // Mesh (nullable)
    void SetMesh(Mesh* mesh) { m_mesh = mesh; m_aabbDirty = true; }
    Mesh* GetMesh() const { return m_mesh; }

    // Material (nullable — nullptr means vertex-color fallback)
    void SetMaterial(Material* material) { m_material = material; }
    Material* GetMaterial() const { return m_material; }

    // World-space AABB (cached, recomputed when dirty)
    DirectX::BoundingBox GetWorldAABB() const;

private:
    std::string m_name;
    Transform m_localTransform;
    Mesh* m_mesh = nullptr;
    Material* m_material = nullptr;
    SceneNode* m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;

    mutable DirectX::BoundingBox m_worldAABB;
    mutable bool m_aabbDirty = true;
};

} // namespace RRE
