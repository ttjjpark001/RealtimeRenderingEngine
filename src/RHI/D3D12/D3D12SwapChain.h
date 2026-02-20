#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "Core/Types.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

namespace RRE
{

class D3D12SwapChain
{
public:
    static constexpr uint32 BUFFER_COUNT = 2;

    D3D12SwapChain() = default;
    ~D3D12SwapChain() = default;

    bool Initialize(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue,
        HWND hwnd, uint32 width, uint32 height, ID3D12Device* device);
    void Shutdown();

    void Present(uint32 syncInterval = 1);
    void ResizeBuffers(uint32 width, uint32 height, ID3D12Device* device);

    uint32 GetCurrentBackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetCurrentBackBuffer() const { return m_backBuffers[GetCurrentBackBufferIndex()].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;

    uint32 GetWidth() const { return m_width; }
    uint32 GetHeight() const { return m_height; }

private:
    void CreateRTVs(ID3D12Device* device);
    void ReleaseBackBuffers();

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
    D3D12DescriptorHeap m_rtvHeap;
    uint32 m_width = 0;
    uint32 m_height = 0;
};

} // namespace RRE
