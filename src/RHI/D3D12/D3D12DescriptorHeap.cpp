#include "RHI/D3D12/D3D12DescriptorHeap.h"
#include <cassert>

namespace RRE
{

bool D3D12DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32 numDescriptors, bool shaderVisible)
{
    m_numDescriptors = numDescriptors;
    m_currentIndex = 0;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    if (FAILED(hr))
        return false;

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::Allocate()
{
    assert(m_currentIndex < m_numDescriptors && "DescriptorHeap overflow: no more descriptors available");
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUStart();
    handle.ptr += static_cast<SIZE_T>(m_currentIndex) * m_descriptorSize;
    m_currentIndex++;
    return handle;
}

void D3D12DescriptorHeap::Reset()
{
    m_currentIndex = 0;
}

} // namespace RRE
