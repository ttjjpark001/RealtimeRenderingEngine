#include "Core/Engine.h"
#include "Platform/Win32/Win32Window.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "RHI/D3D12/D3D12Context.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/DebugHUD.h"
#include "Lighting/PointLight.h"
#include "Platform/Win32/Win32Menu.h"
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

    // Create all 4 mesh types
    m_sphereMesh = std::make_unique<Mesh>(MeshFactory::CreateSphere());
    m_tetrahedronMesh = std::make_unique<Mesh>(MeshFactory::CreateTetrahedron());
    m_cubeMesh = std::make_unique<Mesh>(MeshFactory::CreateCube());
    m_cylinderMesh = std::make_unique<Mesh>(MeshFactory::CreateCylinder());
    m_currentMesh = m_cubeMesh.get();
    UploadMesh(*m_currentMesh);

    // Create menu
    m_menu = std::make_unique<Win32Menu>();
    m_menu->Initialize(m_window->GetHWND());
    m_menu->SetViewCallback([this](uint32 w, uint32 h, bool fullscreen) {
        OnViewModeChanged(w, h, fullscreen);
    });
    m_menu->SetMeshCallback([this](MeshType type) {
        OnMeshTypeChanged(type);
    });
    m_menu->SetAnimCallback([this]() {
        OnAnimationToggle();
    });
    m_window->SetMenu(m_menu.get());

    // Set key callback for Space key animation toggle
    m_window->SetKeyCallback([this](WPARAM key) {
        if (key == VK_SPACE)
            OnAnimationToggle();
    });

    // Create point light
    m_pointLight = std::make_unique<PointLight>();

    // Set light menu callbacks
    m_menu->SetLightColorCallback([this](float r, float g, float b) {
        m_pointLight->SetColor({ r, g, b });
    });
    m_menu->SetLightToggleInfoCallback([this]() {
        m_showLightInfo = !m_showLightInfo;
    });
    m_menu->SetLightResetCallback([this]() {
        m_pointLight->Reset();
    });

    // Create debug HUD
    m_debugHUD = std::make_unique<DebugHUD>();

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
    m_currentMesh = nullptr;
    m_sphereMesh.reset();
    m_tetrahedronMesh.reset();
    m_cubeMesh.reset();
    m_cylinderMesh.reset();
    m_pointLight.reset();
    m_menu.reset();
    m_rhiDevice.reset();
    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // Rotate object (only when animating)
    if (m_isAnimating)
        m_rotationAngle += 1.0f * deltaTime;

    // Move light with arrow keys and PgUp/PgDn
    if (m_pointLight)
    {
        float speed = 3.0f * deltaTime;
        XMFLOAT3 pos = m_pointLight->GetPosition();

        if (GetAsyncKeyState(VK_LEFT) & 0x8000)   pos.x -= speed;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)  pos.x += speed;
        if (GetAsyncKeyState(VK_UP) & 0x8000)     pos.z += speed;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)   pos.z -= speed;
        if (GetAsyncKeyState(VK_PRIOR) & 0x8000)  pos.y += speed;  // PgUp
        if (GetAsyncKeyState(VK_NEXT) & 0x8000)   pos.y -= speed;  // PgDn

        m_pointLight->SetPosition(pos);
    }

    // Update debug HUD
    if (m_debugHUD)
    {
        RenderStats stats = {};
        stats.fps = 0.0f; // DebugHUD calculates this internally
        stats.width = m_window->GetWidth();
        stats.height = m_window->GetHeight();
        stats.aspectRatio = static_cast<float>(stats.width) / static_cast<float>(stats.height);
        stats.totalPolygons = m_indexCount / 3;
        stats.polygonsPerSec = stats.totalPolygons * (1.0f / deltaTime);
        stats.showLightInfo = m_showLightInfo;
        if (m_pointLight)
        {
            stats.lightColorName = m_pointLight->GetColorName();
            stats.lightPosition = m_pointLight->GetPosition();
        }
        m_debugHUD->Update(deltaTime, stats);
    }
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

    // Set lighting data
    if (m_pointLight)
    {
        XMFLOAT3 camPos;
        XMStoreFloat3(&camPos, eyePos);
        XMFLOAT3 ambient = { 0.15f, 0.15f, 0.15f };
        context->SetLightData(
            m_pointLight->GetPosition(), m_pointLight->GetColor(),
            camPos, ambient,
            m_pointLight->GetConstantAttenuation(),
            m_pointLight->GetLinearAttenuation(),
            m_pointLight->GetQuadraticAttenuation());
    }

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

    // Render debug HUD (before EndFrame so text commands are queued)
    if (m_debugHUD)
    {
        m_debugHUD->Render(*context);
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

void Engine::OnViewModeChanged(uint32 width, uint32 height, bool fullscreen)
{
    if (fullscreen)
    {
        m_window->SetFullscreen();
    }
    else
    {
        m_window->SetWindowed(width, height);
    }

    OnResize(m_window->GetWidth(), m_window->GetHeight());
}

void Engine::OnMeshTypeChanged(MeshType type)
{
    switch (type)
    {
    case MeshType::Sphere:      m_currentMesh = m_sphereMesh.get(); break;
    case MeshType::Tetrahedron: m_currentMesh = m_tetrahedronMesh.get(); break;
    case MeshType::Cube:        m_currentMesh = m_cubeMesh.get(); break;
    case MeshType::Cylinder:    m_currentMesh = m_cylinderMesh.get(); break;
    }

    if (m_currentMesh)
        UploadMesh(*m_currentMesh);
}

void Engine::OnAnimationToggle()
{
    m_isAnimating = !m_isAnimating;
    if (m_menu)
        m_menu->UpdateAnimCheckMark(m_isAnimating);
}

} // namespace RRE
