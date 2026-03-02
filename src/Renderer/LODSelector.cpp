#define NOMINMAX
#include "Renderer/LODSelector.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace DirectX;

namespace RRE
{

// ---------------------------------------------------------------------------
// Grid-based vertex clustering LOD generation
//
// Algorithm:
//   1. Divide the mesh AABB into a regular 3D grid whose resolution is chosen
//      to approximate the desired triangle reduction ratio.
//   2. Map every source vertex to its grid cell; all vertices in the same cell
//      share the same representative vertex (the first one encountered).
//   3. Re-index the triangle list; discard triangles whose three corners all
//      collapse to the same cell (degenerate). This produces no holes.
//
// The technique is O(V) and safe to run on a background thread.
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> LODSelector::GenerateLODMesh(const Mesh* source, float targetRatio)
{
    const auto& srcVerts = source->vertices;
    const auto& srcIdx   = source->indices;

    if (srcVerts.empty() || srcIdx.size() < 3)
        return nullptr;

    uint32 srcTriCount = static_cast<uint32>(srcIdx.size() / 3);

    // --- Compute mesh AABB ---
    XMFLOAT3 minP = srcVerts[0].position;
    XMFLOAT3 maxP = srcVerts[0].position;
    for (const auto& v : srcVerts)
    {
        minP.x = std::min(minP.x, v.position.x);
        minP.y = std::min(minP.y, v.position.y);
        minP.z = std::min(minP.z, v.position.z);
        maxP.x = std::max(maxP.x, v.position.x);
        maxP.y = std::max(maxP.y, v.position.y);
        maxP.z = std::max(maxP.z, v.position.z);
    }

    float dx = maxP.x - minP.x;
    float dy = maxP.y - minP.y;
    float dz = maxP.z - minP.z;

    // Grid resolution: cube-root of the target triangle count drives the cell size.
    float targetTriCount = static_cast<float>(srcTriCount) * targetRatio;
    float gridRes = std::max(4.0f, std::cbrtf(targetTriCount));

    // Cell size per axis (guard against degenerate flat meshes).
    constexpr float kEps = 1e-5f;
    float cellX = (dx > kEps) ? (dx / gridRes) : kEps;
    float cellY = (dy > kEps) ? (dy / gridRes) : kEps;
    float cellZ = (dz > kEps) ? (dz / gridRes) : kEps;

    // --- Vertex clustering ---
    struct CellKey
    {
        int32 x, y, z;
        bool operator==(const CellKey& o) const noexcept
        { return x == o.x && y == o.y && z == o.z; }
    };
    struct CellHash
    {
        size_t operator()(const CellKey& k) const noexcept
        {
            size_t h = static_cast<size_t>(k.x);
            h ^= static_cast<size_t>(k.y) * 2654435769ULL
                + 0x9e3779b9ULL + (h << 6) + (h >> 2);
            h ^= static_cast<size_t>(k.z) * 2246822519ULL
                + 0x9e3779b9ULL + (h << 6) + (h >> 2);
            return h;
        }
    };

    std::unordered_map<CellKey, uint32, CellHash> cellToNewIdx;
    std::vector<Vertex>   newVerts;
    std::vector<uint32>   vertRemap(srcVerts.size());

    cellToNewIdx.reserve(srcVerts.size() / 2);
    newVerts.reserve(srcVerts.size() / 2);

    for (uint32 i = 0; i < static_cast<uint32>(srcVerts.size()); i++)
    {
        const Vertex& v = srcVerts[i];
        CellKey key{
            static_cast<int32>((v.position.x - minP.x) / cellX),
            static_cast<int32>((v.position.y - minP.y) / cellY),
            static_cast<int32>((v.position.z - minP.z) / cellZ)
        };

        auto [it, inserted] = cellToNewIdx.emplace(key, static_cast<uint32>(newVerts.size()));
        vertRemap[i] = it->second;
        if (inserted)
            newVerts.push_back(v);
    }

    // --- Re-index triangles, discard degenerates ---
    auto result = std::make_unique<Mesh>();
    result->vertices = std::move(newVerts);
    result->indices.reserve(srcTriCount * 3 / 2);

    for (uint32 t = 0; t < srcTriCount; t++)
    {
        uint32 a = vertRemap[srcIdx[t * 3 + 0]];
        uint32 b = vertRemap[srcIdx[t * 3 + 1]];
        uint32 c = vertRemap[srcIdx[t * 3 + 2]];

        if (a != b && b != c && a != c)
        {
            result->indices.push_back(a);
            result->indices.push_back(b);
            result->indices.push_back(c);
        }
    }

    if (result->indices.empty())
        return nullptr;

    result->aabb = source->aabb;
    return result;
}

// ---------------------------------------------------------------------------

void LODSelector::RegisterMesh(Mesh* mesh, float dist1, float dist2)
{
    if (!mesh || m_entries.count(mesh))
        return;

    auto entry = std::make_unique<Entry>();
    entry->lodMesh.meshLODs[0]        = mesh;
    entry->lodMesh.switchDistances[0] = 0.0f;
    entry->lodMesh.switchDistances[1] = dist1;
    entry->lodMesh.switchDistances[2] = dist2;
    entry->lodMesh.lodCount           = 1;

    Entry* rawEntry = entry.get();
    m_entries[mesh] = std::move(entry);

    // Only generate LODs for meshes complex enough to benefit.
    if (mesh->GetPolygonCount() < kMinTrisForLOD)
        return;

    rawEntry->future = std::async(std::launch::async, [rawEntry, mesh]()
    {
        auto lod1 = GenerateLODMesh(mesh, 0.5f);
        auto lod2 = GenerateLODMesh(mesh, 0.25f);

        if (lod1 && lod1->GetPolygonCount() > 0)
        {
            rawEntry->lodMesh.meshLODs[1] = lod1.get();
            rawEntry->ownedLODs.push_back(std::move(lod1));
        }
        if (lod2 && lod2->GetPolygonCount() > 0)
        {
            // Place at slot 2 if LOD 1 exists, otherwise take slot 1.
            uint32 slot = rawEntry->lodMesh.meshLODs[1] ? 2u : 1u;
            rawEntry->lodMesh.meshLODs[slot] = lod2.get();
            rawEntry->ownedLODs.push_back(std::move(lod2));
        }

        // lodCount = 1 (original) + number of generated LOD levels that were placed
        uint32 extra = (rawEntry->lodMesh.meshLODs[1] ? 1u : 0u)
                     + (rawEntry->lodMesh.meshLODs[2] ? 1u : 0u);
        rawEntry->lodMesh.lodCount = 1u + extra;

        // Release-store: signals the main thread that meshLODs / lodCount are ready.
        rawEntry->lodsReady.store(true, std::memory_order_release);
    });
}

Mesh* LODSelector::SelectLOD(Mesh* originalMesh, float distance) const
{
    auto it = m_entries.find(originalMesh);
    if (it == m_entries.end())
        return originalMesh;

    const Entry* entry = it->second.get();

    // Acquire-load: pairs with the release-store in the background thread.
    if (!entry->lodsReady.load(std::memory_order_acquire))
        return originalMesh;

    const LODMesh& lod = entry->lodMesh;

    // Walk from the coarsest LOD down to find the highest-detail one that fits.
    for (uint32 i = lod.lodCount - 1; i > 0; i--)
    {
        if (distance >= lod.switchDistances[i] && lod.meshLODs[i])
            return lod.meshLODs[i];
    }
    return lod.meshLODs[0];
}

const LODMesh* LODSelector::GetLODMesh(Mesh* originalMesh) const
{
    auto it = m_entries.find(originalMesh);
    return (it != m_entries.end()) ? &it->second->lodMesh : nullptr;
}

void LODSelector::WaitForAll()
{
    for (auto& [mesh, entry] : m_entries)
    {
        if (entry->future.valid())
            entry->future.get();
    }
}

void LODSelector::Clear()
{
    WaitForAll();
    m_entries.clear();
}

} // namespace RRE
