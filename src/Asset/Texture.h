#pragma once

#include "Core/Types.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

namespace RRE
{

enum class TextureState
{
    Pending,
    Loading,
    Ready
};

class Texture
{
public:
    Texture() = default;
    ~Texture() = default;

    // Non-copyable, movable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;

    // Create GPU texture from raw RGBA pixel data
    // Upload buffer is used internally; caller must ensure GPU copy completes before releasing
    bool CreateFromData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                        const uint8* pixels, uint32 width, uint32 height,
                        DXGI_FORMAT format);

    // Create a 1x1 white fallback texture
    static std::unique_ptr<Texture> CreateFallback(ID3D12Device* device,
                                                    ID3D12GraphicsCommandList* cmdList);

    // Accessors
    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    uint32 GetWidth() const { return m_width; }
    uint32 GetHeight() const { return m_height; }
    DXGI_FORMAT GetFormat() const { return m_format; }
    TextureState GetState() const { return m_state; }
    void SetState(TextureState state) { m_state = state; }

    // SRV handle storage (actual SRV creation deferred to Phase 15)
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCpuHandle() const { return m_srvCpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGpuHandle() const { return m_srvGpu; }
    void SetSRVHandles(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        m_srvCpu = cpu;
        m_srvGpu = gpu;
    }

    // Upload buffer must survive until GPU copy completes
    ID3D12Resource* GetUploadBuffer() const { return m_uploadBuffer.Get(); }
    void ReleaseUploadBuffer() { m_uploadBuffer.Reset(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpu = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpu = {};
    uint32 m_width = 0;
    uint32 m_height = 0;
    DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureState m_state = TextureState::Pending;
};

} // namespace RRE
