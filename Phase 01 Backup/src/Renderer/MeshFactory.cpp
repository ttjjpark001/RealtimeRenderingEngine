#define NOMINMAX
#include "Renderer/MeshFactory.h"
#include "Renderer/FaceColorPalette.h"
#include <DirectXMath.h>
#include <map>
#include <array>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace RRE
{

namespace
{

// Compute face normal from three vertices
XMFLOAT3 ComputeFaceNormal(const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2)
{
    XMVECTOR p0 = XMLoadFloat3(&v0);
    XMVECTOR p1 = XMLoadFloat3(&v1);
    XMVECTOR p2 = XMLoadFloat3(&v2);
    XMVECTOR edge1 = XMVectorSubtract(p1, p0);
    XMVECTOR edge2 = XMVectorSubtract(p2, p0);
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));
    XMFLOAT3 result;
    XMStoreFloat3(&result, normal);
    return result;
}

// Build edge-based adjacency from indexed triangles
// An edge is shared if two triangles share exactly two vertex positions
// This uses position-based edge keys for per-face (duplicated) vertices
struct EdgeKey
{
    // Sorted pair of position indices in the original shared-vertex positions
    uint32 a, b;
    bool operator<(const EdgeKey& other) const
    {
        if (a != other.a) return a < other.a;
        return b < other.b;
    }
};

// Build adjacency from face list with shared position indices
std::vector<std::vector<uint32>> BuildAdjacency(
    const std::vector<std::array<uint32, 3>>& facePositionIndices, uint32 faceCount)
{
    std::vector<std::vector<uint32>> adjacency(faceCount);
    // Map from edge to list of faces using that edge
    std::map<EdgeKey, std::vector<uint32>> edgeToFaces;

    for (uint32 f = 0; f < faceCount; ++f)
    {
        const auto& face = facePositionIndices[f];
        for (int e = 0; e < 3; ++e)
        {
            uint32 a = face[e];
            uint32 b = face[(e + 1) % 3];
            EdgeKey key = { std::min(a, b), std::max(a, b) };
            edgeToFaces[key].push_back(f);
        }
    }

    for (auto& pair : edgeToFaces)
    {
        auto& faceList = pair.second;
        for (size_t i = 0; i < faceList.size(); ++i)
        {
            for (size_t j = i + 1; j < faceList.size(); ++j)
            {
                adjacency[faceList[i]].push_back(faceList[j]);
                adjacency[faceList[j]].push_back(faceList[i]);
            }
        }
    }

    // Remove duplicate adjacency entries (sort+unique for cache locality)
    for (auto& adj : adjacency)
    {
        std::sort(adj.begin(), adj.end());
        adj.erase(std::unique(adj.begin(), adj.end()), adj.end());
    }

    return adjacency;
}

// Create a mesh from faces with shared position indices, applying face coloring
Mesh BuildColoredMesh(
    const std::vector<XMFLOAT3>& positions,
    const std::vector<std::array<uint32, 3>>& faces)
{
    uint32 faceCount = static_cast<uint32>(faces.size());
    auto adjacency = BuildAdjacency(faces, faceCount);
    auto faceColors = FaceColorPalette::AssignFaceColors(adjacency);

    Mesh mesh;
    mesh.faceAdjacency = adjacency;

    for (uint32 f = 0; f < faceCount; ++f)
    {
        const auto& face = faces[f];
        XMFLOAT3 normal = ComputeFaceNormal(
            positions[face[0]], positions[face[1]], positions[face[2]]);
        XMFLOAT4 color = FaceColorPalette::GetColor(faceColors[f]);

        uint32 baseIndex = static_cast<uint32>(mesh.vertices.size());

        // Per-face vertices (duplicated for flat shading)
        for (int v = 0; v < 3; ++v)
        {
            Vertex vert;
            vert.position = positions[face[v]];
            vert.color = color;
            vert.normal = normal;
            mesh.vertices.push_back(vert);
        }

        mesh.indices.push_back(baseIndex);
        mesh.indices.push_back(baseIndex + 1);
        mesh.indices.push_back(baseIndex + 2);
    }

    return mesh;
}

} // anonymous namespace

Mesh MeshFactory::CreateTetrahedron()
{
    // Regular tetrahedron vertices
    const float a = 1.0f;
    std::vector<XMFLOAT3> positions = {
        {  a,  a,  a },
        {  a, -a, -a },
        { -a,  a, -a },
        { -a, -a,  a },
    };

    std::vector<std::array<uint32, 3>> faces = {
        { 0, 1, 2 },
        { 0, 3, 1 },
        { 0, 2, 3 },
        { 1, 3, 2 },
    };

    return BuildColoredMesh(positions, faces);
}

Mesh MeshFactory::CreateCube()
{
    std::vector<XMFLOAT3> positions = {
        { -1.0f, -1.0f, -1.0f }, // 0: left  bottom back
        {  1.0f, -1.0f, -1.0f }, // 1: right bottom back
        {  1.0f,  1.0f, -1.0f }, // 2: right top    back
        { -1.0f,  1.0f, -1.0f }, // 3: left  top    back
        { -1.0f, -1.0f,  1.0f }, // 4: left  bottom front
        {  1.0f, -1.0f,  1.0f }, // 5: right bottom front
        {  1.0f,  1.0f,  1.0f }, // 6: right top    front
        { -1.0f,  1.0f,  1.0f }, // 7: left  top    front
    };

    // 6 faces, each split into 2 triangles = 12 triangles
    std::vector<std::array<uint32, 3>> faces = {
        // Front face
        { 4, 5, 6 }, { 4, 6, 7 },
        // Back face
        { 1, 0, 3 }, { 1, 3, 2 },
        // Top face
        { 3, 7, 6 }, { 3, 6, 2 },
        // Bottom face
        { 4, 0, 1 }, { 4, 1, 5 },
        // Right face
        { 5, 1, 2 }, { 5, 2, 6 },
        // Left face
        { 0, 4, 7 }, { 0, 7, 3 },
    };

    return BuildColoredMesh(positions, faces);
}

Mesh MeshFactory::CreateSphere(uint32 segments, uint32 rings)
{
    const float PI = XM_PI;
    const float TWO_PI = XM_2PI;

    // Generate shared positions on the sphere
    std::vector<XMFLOAT3> positions;

    // Top pole
    positions.push_back({ 0.0f, 1.0f, 0.0f });

    // Middle rings
    for (uint32 ring = 1; ring < rings; ++ring)
    {
        float phi = PI * static_cast<float>(ring) / static_cast<float>(rings);
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);

        for (uint32 seg = 0; seg < segments; ++seg)
        {
            float theta = TWO_PI * static_cast<float>(seg) / static_cast<float>(segments);
            float x = sinPhi * cosf(theta);
            float y = cosPhi;
            float z = sinPhi * sinf(theta);
            positions.push_back({ x, y, z });
        }
    }

    // Bottom pole
    positions.push_back({ 0.0f, -1.0f, 0.0f });

    std::vector<std::array<uint32, 3>> faces;

    // Top cap triangles
    for (uint32 seg = 0; seg < segments; ++seg)
    {
        uint32 next = (seg + 1) % segments;
        faces.push_back({ 0, 1 + seg, 1 + next });
    }

    // Middle quads (as 2 triangles each)
    for (uint32 ring = 0; ring < rings - 2; ++ring)
    {
        uint32 ringStart = 1 + ring * segments;
        uint32 nextRingStart = 1 + (ring + 1) * segments;

        for (uint32 seg = 0; seg < segments; ++seg)
        {
            uint32 next = (seg + 1) % segments;
            uint32 a = ringStart + seg;
            uint32 b = ringStart + next;
            uint32 c = nextRingStart + next;
            uint32 d = nextRingStart + seg;

            faces.push_back({ a, d, c });
            faces.push_back({ a, c, b });
        }
    }

    // Bottom cap triangles
    uint32 bottomPole = static_cast<uint32>(positions.size() - 1);
    uint32 lastRingStart = 1 + (rings - 2) * segments;
    for (uint32 seg = 0; seg < segments; ++seg)
    {
        uint32 next = (seg + 1) % segments;
        faces.push_back({ bottomPole, lastRingStart + next, lastRingStart + seg });
    }

    return BuildColoredMesh(positions, faces);
}

Mesh MeshFactory::CreateCylinder(uint32 segments, float height)
{
    float halfH = height / 2.0f;
    const float TWO_PI = XM_2PI;

    std::vector<XMFLOAT3> positions;

    // Top center (index 0)
    positions.push_back({ 0.0f, halfH, 0.0f });

    // Top ring (indices 1 .. segments)
    for (uint32 i = 0; i < segments; ++i)
    {
        float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(segments);
        positions.push_back({ cosf(theta), halfH, sinf(theta) });
    }

    // Bottom center (index segments+1)
    positions.push_back({ 0.0f, -halfH, 0.0f });

    // Bottom ring (indices segments+2 .. 2*segments+1)
    for (uint32 i = 0; i < segments; ++i)
    {
        float theta = TWO_PI * static_cast<float>(i) / static_cast<float>(segments);
        positions.push_back({ cosf(theta), -halfH, sinf(theta) });
    }

    std::vector<std::array<uint32, 3>> faces;

    uint32 topCenter = 0;
    uint32 bottomCenter = segments + 1;
    uint32 topRingStart = 1;
    uint32 bottomRingStart = segments + 2;

    // Top cap
    for (uint32 i = 0; i < segments; ++i)
    {
        uint32 next = (i + 1) % segments;
        faces.push_back({ topCenter, topRingStart + next, topRingStart + i });
    }

    // Bottom cap
    for (uint32 i = 0; i < segments; ++i)
    {
        uint32 next = (i + 1) % segments;
        faces.push_back({ bottomCenter, bottomRingStart + i, bottomRingStart + next });
    }

    // Side quads (2 triangles each)
    for (uint32 i = 0; i < segments; ++i)
    {
        uint32 next = (i + 1) % segments;
        uint32 tl = topRingStart + i;
        uint32 tr = topRingStart + next;
        uint32 bl = bottomRingStart + i;
        uint32 br = bottomRingStart + next;

        faces.push_back({ tl, tr, br });
        faces.push_back({ tl, br, bl });
    }

    return BuildColoredMesh(positions, faces);
}

} // namespace RRE
