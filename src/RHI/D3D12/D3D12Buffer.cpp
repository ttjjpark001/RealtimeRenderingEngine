#include "RHI/D3D12/D3D12Buffer.h"
#include <cstring>

namespace RRE
{

bool D3D12Buffer::Initialize(ID3D12Device* device, const void* data, uint32 size, uint32 stride)
{
    m_device = device;
    m_size = size;
    m_stride = stride;

    // Create upload heap resource
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
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
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
        return false;

    // Upload data
    if (data)
    {
        void* mapped = nullptr;
        D3D12_RANGE readRange = { 0, 0 };
        hr = m_resource->Map(0, &readRange, &mapped);
        if (SUCCEEDED(hr))
        {
            memcpy(mapped, data, size);
            m_resource->Unmap(0, nullptr);
        }
    }

    return true;
}

void D3D12Buffer::SetData(const void* data, uint32 size, uint32 stride)
{
    m_size = size;
    m_stride = stride;

    if (!m_resource || !data)
        return;

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = m_resource->Map(0, &readRange, &mapped);
    if (SUCCEEDED(hr))
    {
        memcpy(mapped, data, size);
        m_resource->Unmap(0, nullptr);
    }
}

D3D12_VERTEX_BUFFER_VIEW D3D12Buffer::GetVertexBufferView() const
{
    D3D12_VERTEX_BUFFER_VIEW view = {};
    view.BufferLocation = m_resource->GetGPUVirtualAddress();
    view.SizeInBytes = m_size;
    view.StrideInBytes = m_stride;
    return view;
}

D3D12_INDEX_BUFFER_VIEW D3D12Buffer::GetIndexBufferView() const
{
    D3D12_INDEX_BUFFER_VIEW view = {};
    view.BufferLocation = m_resource->GetGPUVirtualAddress();
    view.SizeInBytes = m_size;
    view.Format = DXGI_FORMAT_R32_UINT;
    return view;
}

} // namespace RRE
