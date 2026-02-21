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

    D3D12_CPU_DESCRIPTOR_HANDLE Allocate();
    void Reset();

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUStart() const { return m_heap->GetCPUDescriptorHandleForHeapStart(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUStart() const { return m_heap->GetGPUDescriptorHandleForHeapStart(); }
    uint32 GetDescriptorSize() const { return m_descriptorSize; }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32 m_descriptorSize = 0;
    uint32 m_numDescriptors = 0;
    uint32 m_currentIndex = 0;
};

} // namespace RRE
