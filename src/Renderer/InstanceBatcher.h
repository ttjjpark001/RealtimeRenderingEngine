#pragma once

#include "Renderer/Mesh.h"
#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <utility>

namespace RRE
{

class Material;

// Groups SceneNode world matrices by (Mesh*, Material*) to reduce draw calls.
// Usage per frame:
//   1. Clear()
//   2. AddInstance() for each visible node
//   3. Iterate GetBatches() and issue one DrawIndexedInstanced per batch
class InstanceBatcher
{
public:
    struct Batch
    {
        Mesh*     mesh     = nullptr;
        Material* material = nullptr;
        // Transposed world matrices ready for GPU upload
        std::vector<DirectX::XMFLOAT4X4> worlds;
    };

    // Remove all previously collected instances
    void Clear();

    // Add one instance. worldTransposed must already be XMMatrixTranspose'd.
    void AddInstance(Mesh* mesh, Material* material,
                     const DirectX::XMFLOAT4X4& worldTransposed);

    const std::vector<Batch>& GetBatches() const { return m_batches; }

    // Total number of instances across all batches (for stats)
    uint32_t GetTotalInstanceCount() const;

private:
    // Maps (Mesh*, Material*) pair to index in m_batches
    struct PairHash
    {
        size_t operator()(const std::pair<Mesh*, Material*>& p) const noexcept
        {
            size_t h1 = std::hash<Mesh*>{}(p.first);
            size_t h2 = std::hash<Material*>{}(p.second);
            return h1 ^ (h2 * 2654435761u);
        }
    };

    std::unordered_map<std::pair<Mesh*, Material*>, size_t, PairHash> m_batchIndex;
    std::vector<Batch> m_batches;
};

} // namespace RRE
