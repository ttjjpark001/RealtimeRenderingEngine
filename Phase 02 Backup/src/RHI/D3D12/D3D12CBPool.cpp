#include "RHI/D3D12/D3D12CBPool.h"

namespace RRE
{

bool D3D12CBPool::Initialize(ID3D12Device* device, uint32 totalSize)
{
    m_totalSize = totalSize;
    m_halfSize = totalSize / 2;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = totalSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadHeap));

    if (FAILED(hr))
        return false;

    // Persistent map — never unmap
    D3D12_RANGE readRange = { 0, 0 };
    hr = m_uploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedData));
    if (FAILED(hr))
        return false;

    m_gpuBaseAddress = m_uploadHeap->GetGPUVirtualAddress();
    m_currentOffset = 0;
    m_frameIndex = 0;

    return true;
}

CBAllocation D3D12CBPool::Allocate(uint32 size)
{
    // 256-byte alignment
    uint32 alignedSize = (size + 255) & ~255;

    uint32 frameStart = m_frameIndex * m_halfSize;
    uint32 frameEnd = frameStart + m_halfSize;

    if (m_currentOffset + alignedSize > frameEnd)
    {
        // Pool exhausted for this frame
        return {};
    }

    CBAllocation alloc;
    alloc.gpuAddress = m_gpuBaseAddress + m_currentOffset;
    alloc.cpuPtr = m_mappedData + m_currentOffset;

    m_currentOffset += alignedSize;
    return alloc;
}

void D3D12CBPool::ResetFrame(uint32 frameIndex)
{
    m_frameIndex = frameIndex % 2;
    m_currentOffset = m_frameIndex * m_halfSize;
}

void D3D12CBPool::Shutdown()
{
    if (m_uploadHeap && m_mappedData)
    {
        m_uploadHeap->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
    m_uploadHeap.Reset();
}

} // namespace RRE
