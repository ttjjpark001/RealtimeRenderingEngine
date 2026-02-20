#include "Core/Engine.h"
#include "Platform/Win32/Win32Window.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "RHI/D3D12/D3D12Context.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace RRE
{

Engine::Engine() = default;

Engine::~Engine()
{
    Shutdown();
}

bool Engine::Initialize(const EngineInitParams& params)
{
    HINSTANCE hInstance = static_cast<HINSTANCE>(params.platformHandle);

    // Create window
    m_window = std::make_unique<Win32Window>();
    if (!m_window->Initialize(960, 540, "Realtime Rendering Engine", hInstance))
    {
        return false;
    }

    // Set resize callback
    m_window->SetResizeCallback([this](uint32 width, uint32 height) {
        OnResize(width, height);
    });

    m_window->Show(params.showCommand);

    // Create RHI device
    m_rhiDevice = std::make_unique<D3D12Device>();
    if (!m_rhiDevice->Initialize(m_window->GetHWND(), m_window->GetWidth(), m_window->GetHeight()))
    {
        return false;
    }

    // Create cube mesh
    m_cubeMesh = std::make_unique<Mesh>(MeshFactory::CreateCube());
    UploadMesh(*m_cubeMesh);

    // Initialize high-resolution timer
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_timerFrequency = freq.QuadPart;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    m_lastFrameTime = counter.QuadPart;

    m_isInitialized = true;
    return true;
}

void Engine::Run()
{
    while (m_window->IsRunning())
    {
        m_window->ProcessMessages();

        if (!m_window->IsRunning())
            break;

        // Calculate delta time
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float deltaTime = static_cast<float>(currentTime.QuadPart - m_lastFrameTime)
            / static_cast<float>(m_timerFrequency);
        m_lastFrameTime = currentTime.QuadPart;

        Update(deltaTime);
        Render();
    }
}

void Engine::Shutdown()
{
    if (!m_isInitialized)
        return;

    if (m_rhiDevice)
    {
        m_rhiDevice->Shutdown();
    }

    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_cubeMesh.reset();
    m_rhiDevice.reset();
    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // Rotate cube
    m_rotationAngle += 1.0f * deltaTime;
}

void Engine::Render()
{
    if (!m_rhiDevice)
        return;

    auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());

    // Compute view and projection matrices
    // Camera positioned at a comfortable distance to show the full cube
    XMVECTOR eyePos = XMVectorSet(0.0f, 2.5f, -6.0f, 1.0f);
    XMVECTOR lookAt = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, lookAt, up);

    float aspectRatio = static_cast<float>(m_window->GetWidth())
        / static_cast<float>(m_window->GetHeight());
    // Wider FOV (60°) gives more visible area and makes objects appear smaller
    XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f), aspectRatio, 0.1f, 100.0f);

    // Transpose for HLSL column-major layout (standard D3D12 pattern)
    XMMATRIX viewProj = XMMatrixTranspose(view * projection);
    XMFLOAT4X4 viewProjFloat;
    XMStoreFloat4x4(&viewProjFloat, viewProj);
    context->SetViewProjection(viewProjFloat);

    context->BeginFrame();

    // Clear with cobalt blue
    XMFLOAT4 cobaltBlue(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(cobaltBlue);

    // Draw cube with rotation
    if (m_vertexBuffer && m_indexBuffer)
    {
        // Transpose for HLSL column-major layout
        XMMATRIX world = XMMatrixTranspose(XMMatrixRotationY(m_rotationAngle));
        XMFLOAT4X4 worldFloat;
        XMStoreFloat4x4(&worldFloat, world);

        context->DrawPrimitives(m_vertexBuffer.get(), m_indexBuffer.get(), worldFloat);
    }

    context->EndFrame();
}

void Engine::OnResize(uint32 width, uint32 height)
{
    if (m_rhiDevice && width > 0 && height > 0)
    {
        m_rhiDevice->OnResize(width, height);
    }
}

void Engine::UploadMesh(const Mesh& mesh)
{
    auto* d3dDevice = static_cast<D3D12Device*>(m_rhiDevice.get());

    // Create vertex buffer
    auto vb = std::make_unique<D3D12Buffer>();
    uint32 vbSize = static_cast<uint32>(mesh.vertices.size() * sizeof(Vertex));
    vb->Initialize(d3dDevice->GetD3DDevice(), mesh.vertices.data(), vbSize, sizeof(Vertex));
    m_vertexBuffer = std::move(vb);

    // Create index buffer
    auto ib = std::make_unique<D3D12Buffer>();
    uint32 ibSize = static_cast<uint32>(mesh.indices.size() * sizeof(uint32));
    ib->Initialize(d3dDevice->GetD3DDevice(), mesh.indices.data(), ibSize, sizeof(uint32));
    m_indexBuffer = std::move(ib);

    m_indexCount = static_cast<uint32>(mesh.indices.size());
}

} // namespace RRE
