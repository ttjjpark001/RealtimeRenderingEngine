#include "RHI/D3D12/D3D12Device.h"

namespace RRE
{

D3D12Device::~D3D12Device()
{
    Shutdown();
}

bool D3D12Device::Initialize(void* windowHandle, uint32 width, uint32 height)
{
#ifdef _DEBUG
    // Enable debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    // Create DXGI factory
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
        return false;

    // Enumerate adapters and find hardware adapter
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
        m_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND;
        ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Check if adapter supports D3D12
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            __uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
        adapter.Reset();
    }

    if (!adapter)
    {
        // Fall back to WARP adapter
        Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
        m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
        hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device));
        if (FAILED(hr))
            return false;
    }
    else
    {
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device));
        if (FAILED(hr))
            return false;
    }

    return InitializeInternal(windowHandle, width, height);
}

bool D3D12Device::InitializeWARP(void* windowHandle, uint32 width, uint32 height)
{
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr))
        return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter;
    hr = m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
    if (FAILED(hr))
        return false;

    hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device));
    if (FAILED(hr))
        return false;

    return InitializeInternal(windowHandle, width, height);
}

bool D3D12Device::InitializeInternal(void* windowHandle, uint32 width, uint32 height)
{
    // Initialize context (command queue, command list, fence)
    if (!m_context.Initialize(m_device.Get()))
        return false;

    // Initialize swap chain
    HWND hwnd = static_cast<HWND>(windowHandle);
    if (!m_swapChain.Initialize(m_factory.Get(), m_context.GetCommandQueue(),
        hwnd, width, height, m_device.Get()))
        return false;

    m_context.SetSwapChain(&m_swapChain);

    // Create depth buffer
    m_context.CreateDepthBuffer(width, height);

    m_isInitialized = true;
    return true;
}

void D3D12Device::Shutdown()
{
    if (!m_isInitialized)
        return;

    m_context.WaitForGPU();
    m_swapChain.Shutdown();
    m_context.Shutdown();
    m_device.Reset();
    m_factory.Reset();
    m_isInitialized = false;
}

void D3D12Device::OnResize(uint32 width, uint32 height)
{
    if (!m_isInitialized || width == 0 || height == 0)
        return;

    m_context.WaitForGPU();
    m_swapChain.ResizeBuffers(width, height, m_device.Get());
    m_context.CreateDepthBuffer(width, height);
}

bool D3D12Device::CreateDevice(IDXGIAdapter1* adapter)
{
    HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device));
    return SUCCEEDED(hr);
}

} // namespace RRE
