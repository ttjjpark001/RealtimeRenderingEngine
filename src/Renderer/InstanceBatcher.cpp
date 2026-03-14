#include "Renderer/InstanceBatcher.h"
#include "Core/Types.h"

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

uint32_t InstanceBatcher::GetTotalInstanceCount() const
{
    uint32_t total = 0;
    for (const auto& b : m_batches)
        total += static_cast<uint32_t>(b.worlds.size());
    return total;
}

} // namespace RRE
