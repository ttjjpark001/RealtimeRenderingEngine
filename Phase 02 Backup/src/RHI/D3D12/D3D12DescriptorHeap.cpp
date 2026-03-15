#include "RHI/D3D12/D3D12DescriptorHeap.h"
#include <cassert>

namespace RRE
{

bool D3D12DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32 numDescriptors, bool shaderVisible)
{
    m_numDescriptors = numDescriptors;
    m_currentIndex = 0;
    m_hasSplit = false;

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

bool D3D12DescriptorHeap::Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32 persistentCount, uint32 transientCount, bool shaderVisible)
{
    uint32 total = persistentCount + transientCount;
    if (!Initialize(device, type, total, shaderVisible))
        return false;

    m_hasSplit = true;
    m_persistentCount = persistentCount;
    m_persistentIndex = 0;
    m_transientStart = persistentCount;
    m_transientIndex = persistentCount;

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::Allocate()
{
    if (m_hasSplit)
        return AllocateTransient();

    assert(m_currentIndex < m_numDescriptors && "DescriptorHeap overflow: no more descriptors available");
    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUStart();
    handle.ptr += static_cast<SIZE_T>(m_currentIndex) * m_descriptorSize;
    m_currentIndex++;
    return handle;
}

void D3D12DescriptorHeap::Reset()
{
    if (m_hasSplit)
    {
        ResetTransient();
        return;
    }
    m_currentIndex = 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::AllocatePersistent()
{
    assert(m_hasSplit && "AllocatePersistent requires split initialization");
    assert(m_persistentIndex < m_persistentCount && "Persistent descriptor overflow");

    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUStart();
    handle.ptr += static_cast<SIZE_T>(m_persistentIndex) * m_descriptorSize;
    m_persistentIndex++;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::GetGPUHandleForCPU(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const
{
    SIZE_T offset = cpuHandle.ptr - GetCPUStart().ptr;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = GetGPUStart();
    gpuHandle.ptr += offset;
    return gpuHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::AllocateTransient()
{
    assert(m_hasSplit && "AllocateTransient requires split initialization");
    assert(m_transientIndex < m_numDescriptors && "Transient descriptor overflow");

    D3D12_CPU_DESCRIPTOR_HANDLE handle = GetCPUStart();
    handle.ptr += static_cast<SIZE_T>(m_transientIndex) * m_descriptorSize;
    m_transientIndex++;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorHeap::AllocateTransientGPU()
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = AllocateTransient();
    return GetGPUHandleForCPU(cpuHandle);
}

void D3D12DescriptorHeap::ResetTransient()
{
    m_transientIndex = m_transientStart;
}

} // namespace RRE
