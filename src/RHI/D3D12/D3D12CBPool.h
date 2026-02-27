#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Types.h"

namespace RRE
{

struct CBAllocation
{
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    uint8* cpuPtr = nullptr;
};

class D3D12CBPool
{
public:
    D3D12CBPool() = default;
    ~D3D12CBPool() = default;

    // Initialize with total pool size (default 4MB, split into 2 halves for double buffering)
    bool Initialize(ID3D12Device* device, uint32 totalSize = 4 * 1024 * 1024);

    // Allocate a 256-byte aligned slot from the current frame's half
    // Returns null allocation if pool is exhausted
    CBAllocation Allocate(uint32 size);

    // Reset allocation offset for the given frame index (0 or 1)
    // Call at BeginFrame before any Allocate calls
    void ResetFrame(uint32 frameIndex);

    void Shutdown();

    ID3D12Resource* GetResource() const { return m_uploadHeap.Get(); }
    uint32 GetTotalSize() const { return m_totalSize; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadHeap;
    uint8* m_mappedData = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS m_gpuBaseAddress = 0;
    uint32 m_totalSize = 0;
    uint32 m_halfSize = 0;
    uint32 m_currentOffset = 0;
    uint32 m_frameIndex = 0;
};

} // namespace RRE
