#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "Core/Types.h"
#include "RHI/RHIDevice.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12SwapChain.h"

namespace RRE
{

class D3D12Device : public IRHIDevice
{
public:
    D3D12Device() = default;
    ~D3D12Device() override;

    // IRHIDevice interface
    bool Initialize(void* windowHandle, uint32 width, uint32 height) override;
    void Shutdown() override;
    void OnResize(uint32 width, uint32 height) override;
    IRHIContext* GetContext() override { return &m_context; }

    // Initialize with WARP adapter for testing
    bool InitializeWARP(void* windowHandle, uint32 width, uint32 height);

    ID3D12Device* GetD3DDevice() const { return m_device.Get(); }

private:
    bool CreateDevice(IDXGIAdapter1* adapter);
    bool InitializeInternal(void* windowHandle, uint32 width, uint32 height);

    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;

    D3D12Context m_context;
    D3D12SwapChain m_swapChain;

    bool m_isInitialized = false;
};

} // namespace RRE
