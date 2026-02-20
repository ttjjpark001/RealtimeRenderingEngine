#pragma once

#include <d3d12.h>
#include <d3d11on12.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include "Core/Types.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12PipelineState.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

namespace RRE
{

// Constant buffer data passed to the GPU per draw call
struct PerObjectConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
};

class D3D12SwapChain;

struct TextCommand
{
    int x;
    int y;
    std::string text;
    DirectX::XMFLOAT4 color;
};

class D3D12Context : public IRHIContext
{
public:
    D3D12Context() = default;
    ~D3D12Context() override = default;

    bool Initialize(ID3D12Device* device);
    void Shutdown();

    void SetSwapChain(D3D12SwapChain* swapChain) { m_swapChain = swapChain; }

    // D2D text rendering
    bool InitializeD2D(ID3D12Device* device, ID3D12CommandQueue* commandQueue,
        D3D12SwapChain* swapChain);
    void CreateD2DRenderTargets(D3D12SwapChain* swapChain);
    void ReleaseD2DRenderTargets();
    void ShutdownD2D();

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
    bool CreateConstantBuffer();
    void FlushTextCommands();

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

    // Constant buffer (CBV)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
    D3D12DescriptorHeap m_cbvHeap;
    uint8* m_cbData = nullptr;

    // Current frame's view-projection matrix
    DirectX::XMFLOAT4X4 m_viewProjection;

    // D3D11On12 / D2D / DirectWrite
    Microsoft::WRL::ComPtr<ID3D11On12Device> m_d3d11On12Device;
    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11DeviceContext;
    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dDeviceContext;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;

    // Per-back-buffer wrapped resources
    static constexpr uint32 MAX_BACK_BUFFERS = 2;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_wrappedBackBuffers[MAX_BACK_BUFFERS];
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dRenderTargets[MAX_BACK_BUFFERS];

    // Queued text commands
    std::vector<TextCommand> m_textCommands;
    bool m_d2dInitialized = false;
};

} // namespace RRE
