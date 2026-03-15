#include "RHI/D3D12/D3D12SwapChain.h"

namespace RRE
{

bool D3D12SwapChain::Initialize(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue,
    HWND hwnd, uint32 width, uint32 height, ID3D12Device* device)
{
    m_width = width;
    m_height = height;

    // Create RTV descriptor heap
    if (!m_rtvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, BUFFER_COUNT))
        return false;

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BUFFER_COUNT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = factory->CreateSwapChainForHwnd(
        commandQueue,
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );
    if (FAILED(hr))
        return false;

    // Disable Alt+Enter fullscreen toggle
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr))
        return false;

    CreateRTVs(device);
    return true;
}

void D3D12SwapChain::Shutdown()
{
    ReleaseBackBuffers();
    m_swapChain.Reset();
}

void D3D12SwapChain::Present(uint32 syncInterval)
{
    m_swapChain->Present(syncInterval, 0);
}

void D3D12SwapChain::ResizeBuffers(uint32 width, uint32 height, ID3D12Device* device)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    ReleaseBackBuffers();

    HRESULT hr = m_swapChain->ResizeBuffers(
        BUFFER_COUNT,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0
    );
    if (FAILED(hr))
        return;

    CreateRTVs(device);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12SwapChain::GetCurrentRTV() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap.GetCPUStart();
    handle.ptr += static_cast<SIZE_T>(GetCurrentBackBufferIndex()) * m_rtvHeap.GetDescriptorSize();
    return handle;
}

void D3D12SwapChain::CreateRTVs(ID3D12Device* device)
{
    m_rtvHeap.Reset();
    for (uint32 i = 0; i < BUFFER_COUNT; ++i)
    {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap.Allocate();
        device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
    }
}

void D3D12SwapChain::ReleaseBackBuffers()
{
    for (uint32 i = 0; i < BUFFER_COUNT; ++i)
    {
        m_backBuffers[i].Reset();
    }
}

} // namespace RRE
