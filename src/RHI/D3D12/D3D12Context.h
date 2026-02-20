#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Types.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12PipelineState.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

namespace RRE
{

class D3D12SwapChain;

class D3D12Context : public IRHIContext
{
public:
    D3D12Context() = default;
    ~D3D12Context() override = default;

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    void SetSwapChain(D3D12SwapChain* swapChain) { m_swapChain = swapChain; }

    // Set View-Projection matrix for current frame
    void SetViewProjection(const DirectX::XMFLOAT4X4& viewProj) { m_viewProjection = viewProj; }

    // IRHIContext interface
    void BeginFrame() override;
    void EndFrame() override;
    void Clear(const DirectX::XMFLOAT4& color) override;
    void DrawPrimitives(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix) override;
    void DrawText(int x, int y, const char* text,
        const DirectX::XMFLOAT4& color) override;

    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

    void WaitForGPU();
    void CreateDepthBuffer(uint32 width, uint32 height);

private:
    void MoveToNextFrame();

    ID3D12Device* m_device = nullptr;
    D3D12SwapChain* m_swapChain = nullptr;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Fence for GPU synchronization
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    uint64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    // Pipeline state
    D3D12PipelineState m_pipelineState;
    bool m_hasPSO = false;

    // Depth buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12DescriptorHeap m_dsvHeap;

    // Current frame's view-projection matrix
    DirectX::XMFLOAT4X4 m_viewProjection;
};

} // namespace RRE
