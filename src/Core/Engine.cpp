#include "Core/Engine.h"
#include "Platform/Win32/Win32Window.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "RHI/D3D12/D3D12Context.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/Renderer.h"
#include "Renderer/DebugHUD.h"
#include "Lighting/PointLight.h"
#include "Scene/Camera.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
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

    // Build scene graph: root -> parent (rotating) -> child (orbiting)
    m_sceneGraph = std::make_unique<SceneGraph>();
    {
        auto parentNode = std::make_unique<SceneNode>();
        parentNode->SetMesh(m_currentMesh);
        m_parentNode = m_sceneGraph->GetRoot()->AddChild(std::move(parentNode));

        auto childNode = std::make_unique<SceneNode>();
        childNode->SetMesh(m_currentMesh);
        childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
        m_childNode = m_parentNode->AddChild(std::move(childNode));
    }

    // Create renderer
    m_renderer = std::make_unique<Renderer>();
    {
        auto* d3dDevice = static_cast<D3D12Device*>(m_rhiDevice.get());
        auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());
        m_renderer->SetContext(context, d3dDevice->GetD3DDevice());
    }

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

    // Create light indicator sphere (low-poly, uploaded separately from scene meshes)
    m_lightSphereMesh = std::make_unique<Mesh>(MeshFactory::CreateSphere(8, 8));
    {
        auto* d3dDevice = static_cast<D3D12Device*>(m_rhiDevice.get());
        auto vb = std::make_unique<D3D12Buffer>();
        uint32 vbSize = static_cast<uint32>(m_lightSphereMesh->vertices.size() * sizeof(Vertex));
        vb->Initialize(d3dDevice->GetD3DDevice(), m_lightSphereMesh->vertices.data(), vbSize, sizeof(Vertex));
        m_lightSphereVB = std::move(vb);

        auto ib = std::make_unique<D3D12Buffer>();
        uint32 ibSize = static_cast<uint32>(m_lightSphereMesh->indices.size() * sizeof(uint32));
        ib->Initialize(d3dDevice->GetD3DDevice(), m_lightSphereMesh->indices.data(), ibSize, sizeof(uint32));
        m_lightSphereIB = std::move(ib);
    }

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

    // Create camera
    m_camera = std::make_unique<Camera>();

    // Set camera menu callbacks
    m_menu->SetCameraProjectionCallback([this](bool perspective) {
        m_camera->SetProjectionMode(perspective ? ProjectionMode::Perspective : ProjectionMode::Orthographic);
    });
    m_menu->SetCameraToggleInfoCallback([this]() {
        m_showCameraInfo = !m_showCameraInfo;
    });
    m_menu->SetCameraFovCallback([this](float deltaDegrees) {
        m_camera->AdjustFov(deltaDegrees);
    });
    m_menu->SetCameraResetCallback([this]() {
        m_camera->Reset();
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

    m_renderer.reset();
    m_parentNode = nullptr;
    m_childNode = nullptr;
    m_sceneGraph.reset();
    m_currentMesh = nullptr;
    m_sphereMesh.reset();
    m_tetrahedronMesh.reset();
    m_cubeMesh.reset();
    m_cylinderMesh.reset();
    m_lightSphereVB.reset();
    m_lightSphereIB.reset();
    m_lightSphereMesh.reset();
    m_pointLight.reset();
    m_camera.reset();
    m_menu.reset();
    m_rhiDevice.reset();
    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // Rotate parent node (only when animating); child orbits via scene graph hierarchy
    if (m_isAnimating)
        m_rotationAngle += 1.0f * deltaTime;
    if (m_parentNode)
        m_parentNode->GetTransform().SetRotation({ 0.0f, m_rotationAngle, 0.0f });

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

    // Move camera with WASD+QE, adjust FOV with +/-
    if (m_camera)
    {
        float speed = 3.0f * deltaTime;

        if (GetAsyncKeyState('W') & 0x8000) m_camera->MoveForward(speed);
        if (GetAsyncKeyState('S') & 0x8000) m_camera->MoveForward(-speed);
        if (GetAsyncKeyState('A') & 0x8000) m_camera->MoveRight(-speed);
        if (GetAsyncKeyState('D') & 0x8000) m_camera->MoveRight(speed);
        if (GetAsyncKeyState('Q') & 0x8000) m_camera->MoveUp(speed);
        if (GetAsyncKeyState('E') & 0x8000) m_camera->MoveUp(-speed);

        if (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000)  m_camera->AdjustFov(5.0f * deltaTime);
        if (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000)  m_camera->AdjustFov(-5.0f * deltaTime);
    }

    // Update debug HUD
    if (m_debugHUD)
    {
        RenderStats stats = {};
        stats.fps = 0.0f; // DebugHUD calculates this internally
        stats.width = m_window->GetWidth();
        stats.height = m_window->GetHeight();
        stats.aspectRatio = static_cast<float>(stats.width) / static_cast<float>(stats.height);
        stats.totalPolygons = m_sceneGraph ? m_sceneGraph->GetTotalPolygonCount() : 0;
        stats.polygonsPerSec = stats.totalPolygons * (1.0f / deltaTime);
        stats.showLightInfo = m_showLightInfo;
        if (m_pointLight)
        {
            stats.lightColorName = m_pointLight->GetColorName();
            stats.lightPosition = m_pointLight->GetPosition();
        }
        stats.showCameraInfo = m_showCameraInfo;
        if (m_camera)
        {
            stats.projectionModeName = m_camera->GetProjectionModeName();
            stats.cameraPosition = m_camera->GetPosition();
            stats.cameraDirection = m_camera->GetDirection();
            stats.fovDegrees = m_camera->GetFovDegrees();
        }
        m_debugHUD->Update(deltaTime, stats);
    }
}

void Engine::Render()
{
    if (!m_rhiDevice || !m_renderer || !m_sceneGraph)
        return;

    auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());

    float aspectRatio = static_cast<float>(m_window->GetWidth())
        / static_cast<float>(m_window->GetHeight());

    context->BeginFrame();

    // Clear with cobalt blue
    XMFLOAT4 cobaltBlue(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(cobaltBlue);

    // Render all scene objects via SceneGraph traversal
    m_renderer->RenderScene(*m_sceneGraph, *m_camera, m_pointLight.get(), aspectRatio);

    // Render light indicator sphere (unlit, only when light info visible)
    m_renderer->RenderLightIndicator(m_pointLight.get(), m_showLightInfo,
        m_lightSphereVB.get(), m_lightSphereIB.get());

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

    // Update both parent and child scene nodes
    if (m_parentNode) m_parentNode->SetMesh(m_currentMesh);
    if (m_childNode)  m_childNode->SetMesh(m_currentMesh);

    // Clear Renderer mesh cache so new mesh gets uploaded on next frame
    if (m_renderer) m_renderer->ClearMeshCache();
}

void Engine::OnAnimationToggle()
{
    m_isAnimating = !m_isAnimating;
    if (m_menu)
        m_menu->UpdateAnimCheckMark(m_isAnimating);
}

} // namespace RRE
