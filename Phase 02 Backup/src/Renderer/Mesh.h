#pragma once

#include "Renderer/Vertex.h"
#include "Core/Types.h"
#include <DirectXCollision.h>
#include <vector>

namespace RRE
{

class Mesh
{
public:
    Mesh() = default;
    ~Mesh() = default;

    std::vector<Vertex> vertices;
    std::vector<uint32> indices;
    int32 materialIndex = -1;  // Index into SceneData::materials (-1 = no material)

    // Local-space AABB (computed at load time via BoundingBox::CreateFromPoints)
    DirectX::BoundingBox aabb;

    uint32 GetPolygonCount() const
    {
        return static_cast<uint32>(indices.size() / 3);
    }
};

} // namespace RRE
