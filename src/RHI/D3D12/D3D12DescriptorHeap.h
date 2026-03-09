#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Types.h"

namespace RRE
{

class D3D12DescriptorHeap
{
public:
    D3D12DescriptorHeap() = default;
    ~D3D12DescriptorHeap() = default;

    bool Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32 numDescriptors, bool shaderVisible = false);

    // Initialize with persistent/transient split
    // persistentCount descriptors at the front are persistent (texture SRVs etc.)
    // Remaining descriptors are transient (per-frame CBVs)
    bool Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
        uint32 persistentCount, uint32 transientCount, bool shaderVisible);

    // Legacy: allocate from transient region (backwards compatible)
    D3D12_CPU_DESCRIPTOR_HANDLE Allocate();
    void Reset();

    // Persistent allocation (survives across frames)
    D3D12_CPU_DESCRIPTOR_HANDLE AllocatePersistent();
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandleForCPU(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

    // Transient allocation (reset each frame)
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE AllocateTransientGPU();
    void ResetTransient();

    // Persistent index management (for TextureCache reset on scene change)
    uint32 GetPersistentIndex() const { return m_persistentIndex; }
    void SetPersistentIndex(uint32 idx) { m_persistentIndex = idx; }

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUStart() const { return m_heap->GetCPUDescriptorHandleForHeapStart(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUStart() const { return m_heap->GetGPUDescriptorHandleForHeapStart(); }
    uint32 GetDescriptorSize() const { return m_descriptorSize; }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32 m_descriptorSize = 0;
    uint32 m_numDescriptors = 0;
    uint32 m_currentIndex = 0;

    // Persistent/transient split
    uint32 m_persistentCount = 0;
    uint32 m_persistentIndex = 0;
    uint32 m_transientStart = 0;
    uint32 m_transientIndex = 0;
    bool m_hasSplit = false;
};

} // namespace RRE
