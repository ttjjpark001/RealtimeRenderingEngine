#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12SwapChain.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

namespace RRE
{

bool D3D12Context::Initialize(ID3D12Device* device)
{
    m_device = device;

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
        return false;

    // Create command allocator
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator));
    if (FAILED(hr))
        return false;

    // Create command list
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr))
        return false;

    // Command list starts in open state, close it
    m_commandList->Close();

    // Create fence
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
        return false;

    m_fenceValue = 0;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent)
        return false;

    // Initialize pipeline state
    if (m_pipelineState.Initialize(device))
    {
        m_hasPSO = true;
    }

    // Initialize DSV heap
    m_dsvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);

    // Initialize CBV heap and constant buffer
    if (!CreateConstantBuffer())
        return false;

    // Initialize view-projection to identity
    DirectX::XMStoreFloat4x4(&m_viewProjection, DirectX::XMMatrixIdentity());

    return true;
}

bool D3D12Context::CreateConstantBuffer()
{
    // Create CBV_SRV_UAV descriptor heap (shader-visible, one descriptor per draw call)
    if (!m_cbvHeap.Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_DRAW_CALLS, true))
        return false;

    // Constant buffer size must be 256-byte aligned (per slot)
    m_cbAlignedSize = (sizeof(PerObjectConstants) + 255) & ~255;
    UINT totalSize = m_cbAlignedSize * MAX_DRAW_CALLS;

    // Create upload heap buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer));
    if (FAILED(hr))
        return false;

    // Create one CBV descriptor per draw call slot
    D3D12_GPU_VIRTUAL_ADDRESS gpuBase = m_constantBuffer->GetGPUVirtualAddress();
    for (uint32 i = 0; i < MAX_DRAW_CALLS; ++i)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = gpuBase + i * m_cbAlignedSize;
        cbvDesc.SizeInBytes = m_cbAlignedSize;
        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = m_cbvHeap.Allocate();
        m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);
    }

    // Cache CBV descriptor increment size for DrawPrimitives
    m_cbvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Keep the buffer persistently mapped
    hr = m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_cbData));
    if (FAILED(hr))
        return false;

    return true;
}

bool D3D12Context::InitializeD2D(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
    D3D12SwapChain* swapChain)
{
    // Create D3D11On12 device wrapping D3D12
    IUnknown* queues[] = { commandQueue };
    UINT d3d11DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    d3d11DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device;
    HRESULT hr = D3D11On12CreateDevice(
        device,
        d3d11DeviceFlags,
        nullptr, 0,      // feature levels
        queues, 1,       // command queues
        0,               // node mask
        &d3d11Device,
        &m_d3d11DeviceContext,
        nullptr);
    if (FAILED(hr))
        return false;

    hr = d3d11Device.As(&m_d3d11On12Device);
    if (FAILED(hr))
        return false;

    m_d3d11Device = d3d11Device;

    // Create D2D1 factory
    D2D1_FACTORY_OPTIONS d2dOptions = {};
#ifdef _DEBUG
    d2dOptions.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1), &d2dOptions,
        reinterpret_cast<void**>(m_d2dFactory.GetAddressOf()));
    if (FAILED(hr))
        return false;

    // Get DXGI device from D3D11 device
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3d11Device.As(&dxgiDevice);
    if (FAILED(hr))
        return false;

    // Create D2D1 device and device context
    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr))
        return false;

    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dDeviceContext);
    if (FAILED(hr))
        return false;

    // Create DirectWrite factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr))
        return false;

    // Create text format (Consolas 14pt)
    hr = m_dwriteFactory->CreateTextFormat(
        L"Consolas", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        14.0f, L"en-us", &m_textFormat);
    if (FAILED(hr))
        return false;

    // Create D2D render targets for back buffers
    CreateD2DRenderTargets(swapChain);

    // Create brush (will be set per-draw)
    hr = m_d2dDeviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_textBrush);
    if (FAILED(hr))
        return false;

    m_d2dInitialized = true;
    return true;
}

void D3D12Context::CreateD2DRenderTargets(D3D12SwapChain* swapChain)
{
    if (!m_d3d11On12Device || !swapChain)
        return;

    float dpiX = 96.0f, dpiY = 96.0f;

    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY);

    for (uint32 i = 0; i < D3D12SwapChain::BUFFER_COUNT; i++)
    {
        D3D11_RESOURCE_FLAGS d3d11Flags = {};
        d3d11Flags.BindFlags = D3D11_BIND_RENDER_TARGET;

        HRESULT hr = m_d3d11On12Device->CreateWrappedResource(
            swapChain->GetBackBuffer(i),
            &d3d11Flags,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT,
            IID_PPV_ARGS(&m_wrappedBackBuffers[i]));
        if (FAILED(hr))
            continue;

        Microsoft::WRL::ComPtr<IDXGISurface> surface;
        hr = m_wrappedBackBuffers[i].As(&surface);
        if (FAILED(hr))
            continue;

        hr = m_d2dDeviceContext->CreateBitmapFromDxgiSurface(
            surface.Get(), &bitmapProps, &m_d2dRenderTargets[i]);
    }
}

void D3D12Context::ReleaseD2DRenderTargets()
{
    if (m_d2dDeviceContext)
        m_d2dDeviceContext->SetTarget(nullptr);

    for (uint32 i = 0; i < MAX_BACK_BUFFERS; i++)
    {
        m_d2dRenderTargets[i].Reset();
        m_wrappedBackBuffers[i].Reset();
    }

    if (m_d3d11DeviceContext)
        m_d3d11DeviceContext->Flush();
}

void D3D12Context::ShutdownD2D()
{
    ReleaseD2DRenderTargets();

    m_textBrush.Reset();
    m_textFormat.Reset();
    m_d2dDeviceContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
    m_dwriteFactory.Reset();
    m_d3d11On12Device.Reset();
    m_d3d11DeviceContext.Reset();
    m_d3d11Device.Reset();
    m_d2dInitialized = false;
}

void D3D12Context::FlushTextCommands()
{
    if (!m_d2dInitialized || m_textCommands.empty() || !m_swapChain)
        return;

    uint32 backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    if (!m_wrappedBackBuffers[backBufferIndex] || !m_d2dRenderTargets[backBufferIndex])
        return;

    // Acquire the wrapped back buffer for D2D rendering
    ID3D11Resource* wrappedResources[] = { m_wrappedBackBuffers[backBufferIndex].Get() };
    m_d3d11On12Device->AcquireWrappedResources(wrappedResources, 1);

    m_d2dDeviceContext->SetTarget(m_d2dRenderTargets[backBufferIndex].Get());
    m_d2dDeviceContext->BeginDraw();

    for (const auto& cmd : m_textCommands)
    {
        // Convert color
        m_textBrush->SetColor(D2D1::ColorF(cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w));

        // Convert text to wide string
        int wlen = MultiByteToWideChar(CP_UTF8, 0, cmd.text.c_str(), -1, nullptr, 0);
        std::wstring wtext(wlen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, cmd.text.c_str(), -1, &wtext[0], wlen);

        // Draw text
        D2D1_RECT_F layoutRect = D2D1::RectF(
            static_cast<float>(cmd.x),
            static_cast<float>(cmd.y),
            static_cast<float>(m_swapChain->GetWidth()),
            static_cast<float>(m_swapChain->GetHeight()));

        m_d2dDeviceContext->DrawText(
            wtext.c_str(), static_cast<UINT32>(wtext.size()),
            m_textFormat.Get(), &layoutRect, m_textBrush.Get());
    }

    m_d2dDeviceContext->EndDraw();
    m_d2dDeviceContext->SetTarget(nullptr);

    // Release the wrapped back buffer (transitions to PRESENT state)
    m_d3d11On12Device->ReleaseWrappedResources(wrappedResources, 1);
    m_d3d11DeviceContext->Flush();

    m_textCommands.clear();
}

void D3D12Context::Shutdown()
{
    WaitForGPU();

    ShutdownD2D();

    // Unmap constant buffer
    if (m_constantBuffer && m_cbData)
    {
        m_constantBuffer->Unmap(0, nullptr);
        m_cbData = nullptr;
    }

    m_pipelineState.Shutdown();
    m_constantBuffer.Reset();
    m_depthBuffer.Reset();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_commandList.Reset();
    m_commandAllocator.Reset();
    m_fence.Reset();
    m_commandQueue.Reset();
}

void D3D12Context::BeginFrame()
{
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    m_drawCallIndex = 0;
}

void D3D12Context::EndFrame()
{
    bool hasTextCommands = m_d2dInitialized && !m_textCommands.empty();

    if (!hasTextCommands)
    {
        // No text: use standard barrier path
        if (m_swapChain)
        {
            ID3D12Resource* backBuffer = m_swapChain->GetCurrentBackBuffer();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            m_commandList->ResourceBarrier(1, &barrier);
        }
    }
    // When text exists, D3D11On12 handles RT->PRESENT via ReleaseWrappedResources

    // Close command list
    m_commandList->Close();

    // Execute command list
    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    // Flush D2D text commands (acquires wrapped resource in RT, releases in PRESENT)
    if (hasTextCommands)
    {
        FlushTextCommands();
    }

    // Present
    if (m_swapChain)
    {
        m_swapChain->Present(1);
    }

    // Wait for GPU
    WaitForGPU();
}

void D3D12Context::Clear(const DirectX::XMFLOAT4& color)
{
    if (!m_swapChain)
        return;

    ID3D12Resource* backBuffer = m_swapChain->GetCurrentBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain->GetCurrentRTV();

    // Transition back buffer: PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Clear render target
    const float clearColor[] = { color.x, color.y, color.z, color.w };
    m_commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    // Clear depth buffer if it exists
    if (m_depthBuffer)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap.GetCPUStart();
        m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    }
    else
    {
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }

    // Set viewport and scissor rect
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_swapChain->GetWidth());
    viewport.Height = static_cast<float>(m_swapChain->GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(m_swapChain->GetWidth());
    scissorRect.bottom = static_cast<LONG>(m_swapChain->GetHeight());
    m_commandList->RSSetScissorRects(1, &scissorRect);
}

void D3D12Context::DrawPrimitives(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4& worldMatrix)
{
    if (!m_hasPSO || !vb || !ib || !m_cbData || m_drawCallIndex >= MAX_DRAW_CALLS)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Write constant buffer to the current slot (zero-init pads all padding fields)
    PerObjectConstants constants = {};
    constants.world = worldMatrix;
    constants.viewProj = m_viewProjection;
    constants.lightPosition = m_lightPosition;
    constants.lightColor = m_lightColor;
    constants.cameraPosition = m_cameraPosition;
    constants.ambientColor = m_ambientColor;
    constants.Kc = m_Kc;
    constants.Kl = m_Kl;
    constants.Kq = m_Kq;
    constants.unlit = m_unlit;
    constants.colorOverride = m_colorOverride;
    memcpy(m_cbData + m_drawCallIndex * m_cbAlignedSize, &constants, sizeof(PerObjectConstants));

    // Set PSO and root signature
    m_commandList->SetPipelineState(m_pipelineState.GetPSO());
    m_commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

    // Set CBV descriptor heap and bind the descriptor for this draw call's slot
    ID3D12DescriptorHeap* heaps[] = { m_cbvHeap.GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_cbvHeap.GetGPUStart();
    gpuHandle.ptr += m_drawCallIndex * m_cbvDescriptorSize;
    m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

    // Set primitive topology
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Set vertex and index buffers
    D3D12_VERTEX_BUFFER_VIEW vbView = d3dVB->GetVertexBufferView();
    m_commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView = d3dIB->GetIndexBufferView();
    m_commandList->IASetIndexBuffer(&ibView);

    // Draw
    uint32 indexCount = d3dIB->GetSize() / sizeof(uint32);
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);

    m_drawCallIndex++;
}

void D3D12Context::DrawText(int x, int y, const char* text,
    const DirectX::XMFLOAT4& color)
{
    if (!m_d2dInitialized || !text)
        return;

    TextCommand cmd;
    cmd.x = x;
    cmd.y = y;
    cmd.text = text;
    cmd.color = color;
    m_textCommands.push_back(std::move(cmd));
}

void D3D12Context::WaitForGPU()
{
    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void D3D12Context::CreateDepthBuffer(uint32 width, uint32 height)
{
    if (width == 0 || height == 0)
        return;

    m_depthBuffer.Reset();

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;

    m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthBuffer));

    // Create DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_dsvHeap.Reset();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap.Allocate();
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, dsvHandle);
}

} // namespace RRE
