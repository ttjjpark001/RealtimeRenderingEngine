#include "Renderer/InstanceBatcher.h"
#include "Core/Types.h"
#include <algorithm>

namespace RRE
{

void InstanceBatcher::Clear()
{
    m_batchIndex.clear();
    m_batches.clear();
}

void InstanceBatcher::AddInstance(Mesh* mesh, Material* material,
                                  const DirectX::XMFLOAT4X4& worldTransposed)
{
    if (!mesh)
        return;

    auto key = std::make_pair(mesh, material);
    auto it = m_batchIndex.find(key);
    if (it != m_batchIndex.end())
    {
        m_batches[it->second].worlds.push_back(worldTransposed);
    }
    else
    {
        size_t idx = m_batches.size();
        m_batchIndex[key] = idx;

        Batch batch;
        batch.mesh     = mesh;
        batch.material = material;
        batch.worlds.push_back(worldTransposed);
        m_batches.push_back(std::move(batch));
    }
}

void InstanceBatcher::SortFrontToBack(const DirectX::XMFLOAT3& cameraPos)
{
    if (m_batches.size() < 2)
        return;

    // Representative position of a batch: translation extracted from the first
    // instance's transposed world matrix. After XMMatrixTranspose(), the original
    // row-4 translation (tx, ty, tz) moves to column 4: _14, _24, _34.
    auto distSq = [&](const Batch& b) -> float {
        if (b.worlds.empty()) return 0.0f;
        const DirectX::XMFLOAT4X4& w = b.worlds[0];
        float dx = w._14 - cameraPos.x;
        float dy = w._24 - cameraPos.y;
        float dz = w._34 - cameraPos.z;
        return dx * dx + dy * dy + dz * dz;
    };

    std::sort(m_batches.begin(), m_batches.end(),
        [&](const Batch& a, const Batch& b) { return distSq(a) < distSq(b); });

    // Rebuild batchIndex to match the new order
    m_batchIndex.clear();
    for (size_t i = 0; i < m_batches.size(); ++i)
        m_batchIndex[{ m_batches[i].mesh, m_batches[i].material }] = i;
}

uint32_t InstanceBatcher::GetTotalInstanceCount() const
{
    uint32_t total = 0;
    for (const auto& b : m_batches)
        total += static_cast<uint32_t>(b.worlds.size());
    return total;
}

} // namespace RRE
