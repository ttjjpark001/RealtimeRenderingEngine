#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12SwapChain.h"
#include "RHI/D3D12/D3D12Buffer.h"

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

    // Initialize view-projection to identity
    DirectX::XMStoreFloat4x4(&m_viewProjection, DirectX::XMMatrixIdentity());

    return true;
}

void D3D12Context::Shutdown()
{
    WaitForGPU();

    m_pipelineState.Shutdown();
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
}

void D3D12Context::EndFrame()
{
    // Transition back buffer: RENDER_TARGET -> PRESENT
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

    // Close command list
    m_commandList->Close();

    // Execute command list
    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, commandLists);

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
    if (!m_hasPSO || !vb || !ib)
        return;

    auto* d3dVB = static_cast<D3D12Buffer*>(vb);
    auto* d3dIB = static_cast<D3D12Buffer*>(ib);

    // Set PSO and root signature
    m_commandList->SetPipelineState(m_pipelineState.GetPSO());
    m_commandList->SetGraphicsRootSignature(m_pipelineState.GetRootSignature());

    // Set root constants: World (16 floats) + ViewProjection (16 floats)
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &worldMatrix, 0);
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &m_viewProjection, 16);

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

void D3D12Context::DrawText(int x, int y, const char* text,
    const DirectX::XMFLOAT4& color)
{
    // TODO: Implement in Phase 6
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(text);
    UNREFERENCED_PARAMETER(color);
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
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
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
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_dsvHeap.Reset();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap.Allocate();
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, dsvHandle);
}

void D3D12Context::MoveToNextFrame()
{
    WaitForGPU();
}

} // namespace RRE
