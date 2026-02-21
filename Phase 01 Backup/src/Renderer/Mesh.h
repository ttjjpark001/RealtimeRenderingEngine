#pragma once

#include "Renderer/Vertex.h"
#include "Core/Types.h"
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

    // Adjacency: for each face i, adjacency[i] is a list of adjacent face indices
    std::vector<std::vector<uint32>> faceAdjacency;

    uint32 GetPolygonCount() const
    {
        return static_cast<uint32>(indices.size() / 3);
    }
};

} // namespace RRE
