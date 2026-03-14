#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12SwapChain.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Asset/Material.h"
#include "Asset/TextureCache.h"
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

    // Initialize CBV+SRV descriptor heap (shader-visible, persistent + transient)
    if (!m_cbvSrvHeap.Initialize(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        PERSISTENT_DESCRIPTORS, TRANSIENT_DESCRIPTORS, true))
        return false;

    m_cbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Initialize constant buffer pool (4MB, double-buffered)
    if (!m_cbPool.Initialize(device))
        return false;

    // Initialize instance data pool (Upload Heap, 4MB total, 2MB per frame)
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Alignment = 0;
        resDesc.Width     = static_cast<UINT64>(INSTANCE_POOL_HALF_SIZE) * 2;
        resDesc.Height    = 1;
        resDesc.DepthOrArraySize = 1;
        resDesc.MipLevels = 1;
        resDesc.Format    = DXGI_FORMAT_UNKNOWN;
        resDesc.SampleDesc.Count = 1;
        resDesc.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resDesc.Flags     = D3D12_RESOURCE_FLAG_NONE;

        hr = device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_instancePool));
        if (FAILED(hr))
            return false;

        m_instancePool->Map(0, nullptr, reinterpret_cast<void**>(&m_instancePoolMapped));
        m_instancePoolGpuBase = m_instancePool->GetGPUVirtualAddress();
    }

    // Initialize Copy Queue for async resource uploads
    {
        D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
        copyQueueDesc.Type  = D3D12_COMMAND_LIST_TYPE_COPY;
        copyQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        hr = device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyQueue));
        if (FAILED(hr))
            return false;

        hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
            IID_PPV_ARGS(&m_copyCommandAllocator));
        if (FAILED(hr))
            return false;

        hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY,
            m_copyCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_copyCommandList));
        if (FAILED(hr))
            return false;

        m_copyCommandList->Close(); // start closed

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence));
        if (FAILED(hr))
            return false;

        m_copyFenceValue = 0;
        m_copyFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_copyFenceEvent)
            return false;
    }

    // Initialize view-projection to identity
    DirectX::XMStoreFloat4x4(&m_viewProjection, DirectX::XMMatrixIdentity());

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
    WaitForCopyQueue();

    ShutdownD2D();

    m_cbPool.Shutdown();
    m_pipelineState.Shutdown();
    m_depthBuffer.Reset();

    // Release shadow map resources
    for (uint32 i = 0; i < MAX_SHADOW_MAPS; i++)
        m_shadowMaps[i].Reset();
    m_shadowMapsCreated = false;

    // Release instance pool
    if (m_instancePool && m_instancePoolMapped)
    {
        m_instancePool->Unmap(0, nullptr);
        m_instancePoolMapped = nullptr;
    }
    m_instancePool.Reset();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    if (m_copyFenceEvent)
    {
        CloseHandle(m_copyFenceEvent);
        m_copyFenceEvent = nullptr;
    }

    m_commandList.Reset();
    m_commandAllocator.Reset();
    m_fence.Reset();
    m_commandQueue.Reset();

    m_copyCommandList.Reset();
    m_copyCommandAllocator.Reset();
    m_copyFence.Reset();
    m_copyQueue.Reset();
}

void D3D12Context::BeginFrame()
{
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // Reset CBPool and transient descriptors for this frame
    m_cbPool.ResetFrame(m_frameCounter);
    m_cbvSrvHeap.ResetTransient();

    // Reset instance data pool for this frame
    m_instancePoolFrame  = m_frameCounter & 1u;
    m_instancePoolOffset = 0;
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

    m_frameCounter++;
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
    if (!m_hasPSO || !vb || !ib)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Allocate CB slot from pool (256-byte aligned)
    CBAllocation cbAlloc = m_cbPool.Allocate(sizeof(PerObjectConstants));
    if (!cbAlloc.cpuPtr)
        return;  // Pool exhausted

    // Write constant buffer data
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
    memcpy(cbAlloc.cpuPtr, &constants, sizeof(PerObjectConstants));

    // Create a transient CBV descriptor for this draw call
    D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvSrvHeap.AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle = m_cbvSrvHeap.GetGPUHandleForCPU(cbvCpuHandle);

    UINT cbAlignedSize = (sizeof(PerObjectConstants) + 255) & ~255;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = cbAlloc.gpuAddress;
    cbvDesc.SizeInBytes = cbAlignedSize;
    m_device->CreateConstantBufferView(&cbvDesc, cbvCpuHandle);

    // Set PSO and root signature
    m_commandList->SetPipelineState(m_pipelineState.GetPSO());
    m_commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

    // Set descriptor heap and bind CBV
    ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, cbvGpuHandle);

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
}

// Forward declaration — definition follows further below
static void BindAndDrawPBR(
    ID3D12GraphicsCommandList*, ID3D12Device*,
    D3D12Buffer*, D3D12Buffer*,
    D3D12_GPU_VIRTUAL_ADDRESS, uint32,
    D3D12CBPool&, D3D12DescriptorHeap&, UINT,
    const DirectX::XMFLOAT4X4&, const DirectX::XMFLOAT3&,
    const LightConstants&, const ShadowConstants&,
    Material*, TextureCache*, bool, int, bool,
    ID3D12PipelineState*, ID3D12RootSignature*,
    const D3D12_CPU_DESCRIPTOR_HANDLE[8], bool);

void D3D12Context::DrawPrimitivesWireframe(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4& worldMatrix)
{
    if (!m_pipelineState.HasWireframePSO() || !vb || !ib)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Allocate CB slot for WireframeConstants (world + viewProj + camPos)
    CBAllocation cbAlloc = m_cbPool.Allocate(sizeof(WireframeConstants));
    if (!cbAlloc.cpuPtr)
        return;

    WireframeConstants constants = {};
    constants.world = worldMatrix;
    constants.viewProj = m_viewProjection;
    constants.cameraPosition = m_cameraPosition;
    memcpy(cbAlloc.cpuPtr, &constants, sizeof(WireframeConstants));

    D3D12_CPU_DESCRIPTOR_HANDLE cbvCpuHandle = m_cbvSrvHeap.AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE cbvGpuHandle = m_cbvSrvHeap.GetGPUHandleForCPU(cbvCpuHandle);

    UINT cbAlignedSize = (sizeof(WireframeConstants) + 255) & ~255;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = cbAlloc.gpuAddress;
    cbvDesc.SizeInBytes = cbAlignedSize;
    m_device->CreateConstantBufferView(&cbvDesc, cbvCpuHandle);

    m_commandList->SetPipelineState(m_pipelineState.GetWireframePSO());
    m_commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

    ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, cbvGpuHandle);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbView = d3dVB->GetVertexBufferView();
    m_commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_INDEX_BUFFER_VIEW ibView = d3dIB->GetIndexBufferView();
    m_commandList->IASetIndexBuffer(&ibView);

    uint32 indexCount = d3dIB->GetSize() / sizeof(uint32);
    m_commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void D3D12Context::DrawPrimitivesPBR(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4& worldMatrix,
    Material* material, TextureCache* textureCache)
{
    // Delegate to the instanced path with a 1-element instance buffer
    DrawPrimitivesPBRInstanced(vb, ib, &worldMatrix, 1, material, textureCache);
}

void D3D12Context::DrawPrimitivesPBRInstanced(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4* worlds, uint32 instanceCount,
    Material* material, TextureCache* textureCache)
{
    if (!m_hasPSO || !vb || !ib || !m_pipelineState.HasPBRPSO() || instanceCount == 0)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Allocate instance data from the per-frame pool
    InstanceAlloc inst = AllocateInstanceBuffer(instanceCount);
    if (!inst.cpuPtr)
        return;

    // Copy transposed world matrices into the pool
    memcpy(inst.cpuPtr, worlds, instanceCount * sizeof(DirectX::XMFLOAT4X4));

    // Choose PSO: alpha-blend uses separate blend state
    ID3D12PipelineState* pso =
        (material && material->alphaMode == AlphaMode::Blend && m_pipelineState.HasPBRAlphaBlendPSO())
        ? m_pipelineState.GetPBRAlphaBlendPSO()
        : m_pipelineState.GetPBRPSO();

    BindAndDrawPBR(
        m_commandList.Get(), m_device,
        d3dVB, d3dIB,
        inst.gpuAddr, instanceCount,
        m_cbPool, m_cbvSrvHeap, m_cbvDescriptorSize,
        m_viewProjection, m_cameraPosition,
        m_pbrLightConstants, m_shadowConstants,
        material, textureCache,
        (material && material->alphaMode == AlphaMode::Blend),
        m_renderModeInt, m_mipMappingEnabled,
        pso, m_pipelineState.GetRootSignature(),
        m_shadowSrvCpu, m_shadowMapsCreated);
}

void D3D12Context::SetShadowMapSize(uint32 size)
{
    // Clamp to [512, 4096] and snap to nearest power-of-two
    if (size <= 512)       size = 512;
    else if (size <= 1024) size = 1024;
    else if (size <= 2048) size = 2048;
    else                   size = 4096;
    m_shadowMapSize = size;
}

void D3D12Context::RecreateShadowMaps()
{
    // Release existing shadow map resources so CreateShadowMaps() rebuilds them
    // at the current m_shadowMapSize.  Caller must have called WaitForGPU() first.
    for (uint32 i = 0; i < MAX_SHADOW_MAPS; i++)
        m_shadowMaps[i].Reset();
    m_shadowMapsCreated = false;
    CreateShadowMaps();
}

void D3D12Context::CreateShadowMaps()
{
    if (m_shadowMapsCreated || !m_device)
        return;

    // Initialize shadow DSV heap (8 descriptors)
    m_shadowDsvHeap.Initialize(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_SHADOW_MAPS);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = m_shadowMapSize;
    texDesc.Height = m_shadowMapSize;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    for (uint32 i = 0; i < MAX_SHADOW_MAPS; i++)
    {
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue,
            IID_PPV_ARGS(&m_shadowMaps[i]));
        if (FAILED(hr))
            return;

        // Create DSV
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_shadowDsvHeap.Allocate();
        m_device->CreateDepthStencilView(m_shadowMaps[i].Get(), &dsvDesc, dsvHandle);

        // Create SRV in persistent region of cbvSrvHeap (must NOT use transient: it resets each frame)
        m_shadowSrvCpu[i] = m_cbvSrvHeap.AllocatePersistent();
        m_shadowSrvGpu[i] = m_cbvSrvHeap.GetGPUHandleForCPU(m_shadowSrvCpu[i]);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_shadowMaps[i].Get(), &srvDesc, m_shadowSrvCpu[i]);
    }

    m_shadowMapsCreated = true;
}

void D3D12Context::BeginShadowPass(uint32 shadowIndex)
{
    if (shadowIndex >= MAX_SHADOW_MAPS || !m_shadowMapsCreated || !m_pipelineState.HasShadowDepthPSO())
        return;

    // Transition shadow map: PIXEL_SHADER_RESOURCE → DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_shadowMaps[shadowIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Get DSV handle for this shadow map
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_shadowDsvHeap.GetCPUStart();
    dsv.ptr += static_cast<SIZE_T>(shadowIndex) *
        m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // Clear depth
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set render target: depth-only (no RTV)
    m_commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

    // Set viewport and scissor for shadow map
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_shadowMapSize);
    viewport.Height = static_cast<float>(m_shadowMapSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.right = static_cast<LONG>(m_shadowMapSize);
    scissor.bottom = static_cast<LONG>(m_shadowMapSize);
    m_commandList->RSSetScissorRects(1, &scissor);

    // Set Shadow Depth PSO and root signature
    m_commandList->SetPipelineState(m_pipelineState.GetShadowDepthPSO());
    m_commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

    // Set descriptor heap (needed for root signature)
    ID3D12DescriptorHeap* heaps[] = { m_cbvSrvHeap.GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D12Context::EndShadowPass(uint32 shadowIndex)
{
    if (shadowIndex >= MAX_SHADOW_MAPS || !m_shadowMapsCreated)
        return;

    // Transition shadow map: DEPTH_WRITE → PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_shadowMaps[shadowIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
}

void D3D12Context::RestoreMainRenderTarget()
{
    if (!m_swapChain)
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain->GetCurrentRTV();

    if (m_depthBuffer)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap.GetCPUStart();
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    }
    else
    {
        m_commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    }

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_swapChain->GetWidth());
    viewport.Height = static_cast<float>(m_swapChain->GetHeight());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {};
    scissor.right = static_cast<LONG>(m_swapChain->GetWidth());
    scissor.bottom = static_cast<LONG>(m_swapChain->GetHeight());
    m_commandList->RSSetScissorRects(1, &scissor);
}

void D3D12Context::DrawShadowDepth(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4& worldMatrix,
    const DirectX::XMFLOAT4X4& lightViewProj)
{
    DrawShadowDepthInstanced(vb, ib, &worldMatrix, 1, lightViewProj);
}

void D3D12Context::DrawShadowDepthInstanced(IRHIBuffer* vb, IRHIBuffer* ib,
    const DirectX::XMFLOAT4X4* worlds, uint32 instanceCount,
    const DirectX::XMFLOAT4X4& lightViewProj)
{
    if (!vb || !ib || !m_pipelineState.HasShadowDepthPSO() || instanceCount == 0)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Allocate instance data from the per-frame pool
    InstanceAlloc inst = AllocateInstanceBuffer(instanceCount);
    if (!inst.cpuPtr)
        return;
    memcpy(inst.cpuPtr, worlds, instanceCount * sizeof(DirectX::XMFLOAT4X4));

    // CB0: ShadowPassConstants (lightViewProj only — world is per-instance)
    CBAllocation cbAlloc = m_cbPool.Allocate(sizeof(ShadowPassConstants));
    if (!cbAlloc.cpuPtr) return;

    ShadowPassConstants constants = {};
    constants.lightViewProj = lightViewProj;
    memcpy(cbAlloc.cpuPtr, &constants, sizeof(ShadowPassConstants));

    D3D12_CPU_DESCRIPTOR_HANDLE cbvCpu = m_cbvSrvHeap.AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu = m_cbvSrvHeap.GetGPUHandleForCPU(cbvCpu);
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = cbAlloc.gpuAddress;
    cbvDesc.SizeInBytes    = (sizeof(ShadowPassConstants) + 255) & ~255;
    m_device->CreateConstantBufferView(&cbvDesc, cbvCpu);

    m_commandList->SetGraphicsRootDescriptorTable(0, cbvGpu);

    // Vertex buffer (slot 0) + instance buffer (slot 1)
    D3D12_VERTEX_BUFFER_VIEW vbView = d3dVB->GetVertexBufferView();
    m_commandList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_VERTEX_BUFFER_VIEW ibufView = {};
    ibufView.BufferLocation = inst.gpuAddr;
    ibufView.SizeInBytes    = instanceCount * 64u;
    ibufView.StrideInBytes  = 64u;
    m_commandList->IASetVertexBuffers(1, 1, &ibufView);

    D3D12_INDEX_BUFFER_VIEW ibView = d3dIB->GetIndexBufferView();
    m_commandList->IASetIndexBuffer(&ibView);

    uint32 indexCount = d3dIB->GetSize() / sizeof(uint32);
    m_commandList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
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

void D3D12Context::BeginUploadCommands()
{
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
}

void D3D12Context::EndUploadCommands()
{
    m_commandList->Close();
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, cmdLists);
    WaitForGPU();
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

// ---------------------------------------------------------------------------
// Copy Queue API
// ---------------------------------------------------------------------------

void D3D12Context::BeginCopyCommands()
{
    m_copyCommandAllocator->Reset();
    m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr);
    m_copyCommandListOpen = true;
}

void D3D12Context::EndCopyCommands()
{
    if (!m_copyCommandListOpen)
        return;
    m_copyCommandList->Close();
    m_copyCommandListOpen = false;

    ID3D12CommandList* cmdLists[] = { m_copyCommandList.Get() };
    m_copyQueue->ExecuteCommandLists(1, cmdLists);

    m_copyFenceValue++;
    m_copyQueue->Signal(m_copyFence.Get(), m_copyFenceValue);
}

void D3D12Context::WaitForCopyQueue()
{
    if (!m_copyFence)
        return;
    if (m_copyFence->GetCompletedValue() < m_copyFenceValue)
    {
        m_copyFence->SetEventOnCompletion(m_copyFenceValue, m_copyFenceEvent);
        WaitForSingleObject(m_copyFenceEvent, INFINITE);
    }
}

// ---------------------------------------------------------------------------
// Instance Buffer Pool — sub-allocate from per-frame Upload Heap ring buffer
// ---------------------------------------------------------------------------

D3D12Context::InstanceAlloc D3D12Context::AllocateInstanceBuffer(uint32 instanceCount)
{
    if (!m_instancePoolMapped || instanceCount == 0)
        return { 0, nullptr };

    // Each InstanceData is 64 bytes; align to 16 bytes (float4 natural alignment)
    constexpr uint32 INSTANCE_STRIDE = 64;
    uint32 byteSize = instanceCount * INSTANCE_STRIDE;

    // Round up to 16-byte boundary
    byteSize = (byteSize + 15u) & ~15u;

    if (m_instancePoolOffset + byteSize > INSTANCE_POOL_HALF_SIZE)
        return { 0, nullptr };  // Pool exhausted for this frame

    uint32 halfBase = m_instancePoolFrame * INSTANCE_POOL_HALF_SIZE;
    uint32 absoluteOffset = halfBase + m_instancePoolOffset;

    InstanceAlloc alloc;
    alloc.gpuAddr = m_instancePoolGpuBase + absoluteOffset;
    alloc.cpuPtr  = m_instancePoolMapped  + absoluteOffset;

    m_instancePoolOffset += byteSize;
    return alloc;
}

// ---------------------------------------------------------------------------
// Instanced PBR draw helpers (shared CB setup extracted into a lambda below)
// ---------------------------------------------------------------------------

// Internal helper: bind all PBR CBs + SRVs, set PSO, IA; then call DrawIndexedInstanced.
// worlds[] is a pointer to 'instanceCount' transposed XMFLOAT4X4 matrices already
// written into an instance Upload Buffer at 'instanceGpuAddr'.
static void BindAndDrawPBR(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12Device*             device,
    D3D12Buffer*              d3dVB,
    D3D12Buffer*              d3dIB,
    D3D12_GPU_VIRTUAL_ADDRESS instanceGpuAddr,
    uint32                    instanceCount,
    D3D12CBPool&              cbPool,
    D3D12DescriptorHeap&      cbvSrvHeap,
    UINT                      cbvDescSize,
    const DirectX::XMFLOAT4X4& viewProj,
    const DirectX::XMFLOAT3&   camPos,
    const LightConstants&       lights,
    const ShadowConstants&      shadows,
    Material*                   material,
    TextureCache*               textureCache,
    bool                        isAlphaBlend,
    int                         renderModeInt,
    bool                        mipEnabled,
    ID3D12PipelineState*        pso,
    ID3D12RootSignature*        rootSig,
    const D3D12_CPU_DESCRIPTOR_HANDLE shadowSrvCpu[8],
    bool                        shadowMapsCreated)
{
    // CB0: PerObjectPBR (viewProj + camPos — world is in instance buffer)
    CBAllocation cb0 = cbPool.Allocate(sizeof(PerObjectPBR));
    if (!cb0.cpuPtr) return;
    PerObjectPBR perObj = {};
    perObj.viewProj       = viewProj;
    perObj.cameraPosition = camPos;
    memcpy(cb0.cpuPtr, &perObj, sizeof(PerObjectPBR));

    // CB1: LightConstants
    CBAllocation cb1 = cbPool.Allocate(sizeof(LightConstants));
    if (!cb1.cpuPtr) return;
    memcpy(cb1.cpuPtr, &lights, sizeof(LightConstants));

    // CB2: PerMaterialConstants
    CBAllocation cb2 = cbPool.Allocate(sizeof(PerMaterialConstants));
    if (!cb2.cpuPtr) return;
    PerMaterialConstants matConst = {};
    if (material)
    {
        matConst.baseColorFactor       = material->baseColorFactor;
        matConst.metallicFactor        = material->metallicFactor;
        matConst.roughnessFactor       = material->roughnessFactor;
        matConst.alphaCutoff           = material->alphaCutoff;
        matConst.hasAlbedoMap          = material->baseColorTexture           ? 1u : 0u;
        matConst.hasNormalMap          = material->normalTexture               ? 1u : 0u;
        matConst.hasMetallicRoughnessMap = material->metallicRoughnessTexture ? 1u : 0u;
        matConst.hasEmissiveMap        = material->emissiveTexture             ? 1u : 0u;
        matConst.hasOcclusionMap       = material->occlusionTexture            ? 1u : 0u;
        matConst.emissiveFactor        = material->emissiveFactor;
        matConst.alphaMode             = static_cast<uint32>(material->alphaMode);
    }
    else
    {
        matConst.baseColorFactor = { 1.f, 1.f, 1.f, 1.f };
        matConst.metallicFactor  = 0.f;
        matConst.roughnessFactor = 0.5f;
        matConst.alphaMode       = 0;
    }
    matConst.useMips = mipEnabled ? 1u : 0u;

    if (renderModeInt == 1) // Solid
    {
        matConst.hasAlbedoMap = matConst.hasNormalMap =
        matConst.hasMetallicRoughnessMap = matConst.hasEmissiveMap =
        matConst.hasOcclusionMap = 0;
    }
    else if (renderModeInt == 2) // BaseColorOnly
    {
        matConst.hasNormalMap = matConst.hasMetallicRoughnessMap =
        matConst.hasEmissiveMap = matConst.hasOcclusionMap = 0;
    }
    memcpy(cb2.cpuPtr, &matConst, sizeof(PerMaterialConstants));

    // CB3: ShadowConstants
    CBAllocation cb3 = cbPool.Allocate(sizeof(ShadowConstants));
    if (!cb3.cpuPtr) return;
    memcpy(cb3.cpuPtr, &shadows, sizeof(ShadowConstants));

    // --- Transient CBV descriptors ---
    auto makeCBV = [&](CBAllocation& alloc, uint32 size) -> D3D12_GPU_DESCRIPTOR_HANDLE {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu = cbvSrvHeap.AllocateTransient();
        D3D12_CONSTANT_BUFFER_VIEW_DESC d = {};
        d.BufferLocation = alloc.gpuAddress;
        d.SizeInBytes    = (size + 255u) & ~255u;
        device->CreateConstantBufferView(&d, cpu);
        return cbvSrvHeap.GetGPUHandleForCPU(cpu);
    };

    D3D12_GPU_DESCRIPTOR_HANDLE cbv0Gpu = makeCBV(cb0, sizeof(PerObjectPBR));
    D3D12_GPU_DESCRIPTOR_HANDLE cbv1Gpu = makeCBV(cb1, sizeof(LightConstants));
    D3D12_GPU_DESCRIPTOR_HANDLE cbv2Gpu = makeCBV(cb2, sizeof(PerMaterialConstants));
    D3D12_GPU_DESCRIPTOR_HANDLE cbv3Gpu = makeCBV(cb3, sizeof(ShadowConstants));

    // --- 5 contiguous texture SRV descriptors ---
    D3D12_CPU_DESCRIPTOR_HANDLE srvBlockCpu = cbvSrvHeap.AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE srvBlockGpu = cbvSrvHeap.GetGPUHandleForCPU(srvBlockCpu);
    for (int i = 1; i < 5; i++) cbvSrvHeap.AllocateTransient();

    auto getSrvCpu = [&](Texture* tex) -> D3D12_CPU_DESCRIPTOR_HANDLE {
        if (tex && tex->GetResource()) return tex->GetSRVCpuHandle();
        if (textureCache && textureCache->GetFallback()) return textureCache->GetFallback()->GetSRVCpuHandle();
        return {};
    };
    Texture* texs[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    if (material) {
        texs[0] = material->baseColorTexture;
        texs[1] = material->normalTexture;
        texs[2] = material->metallicRoughnessTexture;
        texs[3] = material->emissiveTexture;
        texs[4] = material->occlusionTexture;
    }
    for (int i = 0; i < 5; i++) {
        D3D12_CPU_DESCRIPTOR_HANDLE src = getSrvCpu(texs[i]);
        D3D12_CPU_DESCRIPTOR_HANDLE dst = srvBlockCpu;
        dst.ptr += static_cast<SIZE_T>(i) * cbvDescSize;
        if (src.ptr) device->CopyDescriptorsSimple(1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // --- 8 contiguous shadow map SRV descriptors ---
    D3D12_CPU_DESCRIPTOR_HANDLE shadowSrvBlockCpu = cbvSrvHeap.AllocateTransient();
    D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvBlockGpu = cbvSrvHeap.GetGPUHandleForCPU(shadowSrvBlockCpu);
    for (int i = 1; i < 8; i++) cbvSrvHeap.AllocateTransient();

    for (uint32 i = 0; i < MAX_SHADOW_MAPS; i++) {
        D3D12_CPU_DESCRIPTOR_HANDLE dst = shadowSrvBlockCpu;
        dst.ptr += static_cast<SIZE_T>(i) * cbvDescSize;
        if (shadowMapsCreated && shadowSrvCpu[i].ptr) {
            device->CopyDescriptorsSimple(1, dst, shadowSrvCpu[i], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        } else {
            D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv = {};
            nullSrv.Format                  = DXGI_FORMAT_R32_FLOAT;
            nullSrv.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
            nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            nullSrv.Texture2D.MipLevels     = 1;
            device->CreateShaderResourceView(nullptr, &nullSrv, dst);
        }
    }

    // --- Set PSO + root sig + descriptor heap ---
    cmdList->SetPipelineState(pso);
    cmdList->SetGraphicsRootSignature(rootSig);
    ID3D12DescriptorHeap* heaps[] = { cbvSrvHeap.GetHeap() };
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetGraphicsRootDescriptorTable(0, cbv0Gpu);
    cmdList->SetGraphicsRootDescriptorTable(1, srvBlockGpu);
    cmdList->SetGraphicsRootDescriptorTable(2, cbv1Gpu);
    cmdList->SetGraphicsRootDescriptorTable(3, cbv2Gpu);
    cmdList->SetGraphicsRootDescriptorTable(4, shadowSrvBlockGpu);
    cmdList->SetGraphicsRootDescriptorTable(5, cbv3Gpu);

    // --- IA: vertex buffer (slot 0) + instance buffer (slot 1) ---
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbView = d3dVB->GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbView);

    D3D12_VERTEX_BUFFER_VIEW ibufView = {};
    ibufView.BufferLocation = instanceGpuAddr;
    ibufView.SizeInBytes    = instanceCount * 64u; // sizeof(InstanceData)
    ibufView.StrideInBytes  = 64u;
    cmdList->IASetVertexBuffers(1, 1, &ibufView);

    D3D12_INDEX_BUFFER_VIEW ibView = d3dIB->GetIndexBufferView();
    cmdList->IASetIndexBuffer(&ibView);

    uint32 indexCount = d3dIB->GetSize() / sizeof(uint32);
    cmdList->DrawIndexedInstanced(indexCount, instanceCount, 0, 0, 0);
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
