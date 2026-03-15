#pragma once

#include "Core/Types.h"
#include <DirectXMath.h>
#include <atomic>
#include <future>
#include <memory>
#include <unordered_map>
#include <vector>

namespace RRE
{

class Mesh;

constexpr uint32 MAX_LOD_LEVELS = 3;

// Per-mesh LOD configuration: up to MAX_LOD_LEVELS meshes and switch distances.
struct LODMesh
{
    // meshLODs[0] = original (not owned by this struct)
    // meshLODs[1..N-1] = generated LODs (owned by LODSelector::Entry)
    Mesh* meshLODs[MAX_LOD_LEVELS] = {};

    // Camera distance at which to switch to each LOD level.
    // switchDistances[0] = 0 (always), [1] = dist to switch to LOD 1, [2] = to LOD 2.
    float switchDistances[MAX_LOD_LEVELS] = { 0.0f, 50.0f, 200.0f };

    uint32 lodCount = 1;
};

// Manages LOD meshes for registered originals.
// Auto-generates LOD 1 (~50% triangles) and LOD 2 (~25% triangles) asynchronously
// on background threads. Falls back to LOD 0 until generation is complete.
class LODSelector
{
public:
    LODSelector()  = default;
    ~LODSelector() = default;

    // Register a mesh and start async LOD generation.
    // dist1 / dist2: camera distances at which to switch to LOD 1 / LOD 2.
    // Only generates LODs for meshes with more than kMinTrisForLOD triangles.
    void RegisterMesh(Mesh* mesh, float dist1, float dist2);

    // Returns the best LOD mesh for the given camera–object distance.
    // Falls back to the original mesh while LOD generation is in progress.
    Mesh* SelectLOD(Mesh* originalMesh, float distance) const;

    // Returns the LODMesh descriptor for an original mesh (nullptr if not registered).
    const LODMesh* GetLODMesh(Mesh* originalMesh) const;

    // Wait for all pending background LOD generation to complete.
    void WaitForAll();

    // Remove all LOD registrations and free generated meshes.
    // Calls WaitForAll() first to ensure safe teardown.
    void Clear();

    // Minimum triangle count to qualify for auto-LOD generation.
    static constexpr uint32 kMinTrisForLOD = 200;

    // --- Internal: made public only for testing ---
    static std::unique_ptr<Mesh> GenerateLODMesh(const Mesh* source, float targetRatio);

private:
    struct Entry
    {
        LODMesh lodMesh;
        std::vector<std::unique_ptr<Mesh>> ownedLODs;
        std::future<void>    future;
        std::atomic<bool>    lodsReady{ false };

        // Non-copyable, non-movable (due to atomic<bool>)
        Entry() = default;
        Entry(const Entry&) = delete;
        Entry& operator=(const Entry&) = delete;
    };

    std::unordered_map<Mesh*, std::unique_ptr<Entry>> m_entries;
};

} // namespace RRE
