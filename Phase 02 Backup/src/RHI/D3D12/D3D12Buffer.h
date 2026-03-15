#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Types.h"
#include "RHI/RHIBuffer.h"

namespace RRE
{

class D3D12Buffer : public IRHIBuffer
{
public:
    D3D12Buffer() = default;
    ~D3D12Buffer() override = default;

    bool Initialize(ID3D12Device* device, const void* data, uint32 size, uint32 stride);

    // IRHIBuffer interface
    void SetData(const void* data, uint32 size, uint32 stride) override;
    uint32 GetSize() const override { return m_size; }
    uint32 GetStride() const override { return m_stride; }

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    uint32 GetElementCount() const { return m_stride > 0 ? m_size / m_stride : 0; }

private:
    ID3D12Device* m_device = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    uint32 m_size = 0;
    uint32 m_stride = 0;
};

} // namespace RRE
