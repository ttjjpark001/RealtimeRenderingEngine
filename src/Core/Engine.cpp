#include "Core/Engine.h"
#include "Core/AnimationController.h"
#include "Platform/Win32/Win32Window.h"
#include "RHI/RHIDevice.h"
#include "RHI/RHIContext.h"
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "RHI/D3D12/D3D12Context.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/DebugHUD.h"
#include "Lighting/LightManager.h"
#include "Lighting/Light.h"
#include "Scene/Camera.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Platform/Win32/Win32Menu.h"
#include "Asset/SceneLoader.h"
#include "Asset/Material.h"
#include "Asset/TextureCache.h"
#include "Asset/TextureStreamer.h"
#include "Core/ThreadPool.h"
#include <DirectXMath.h>
#include <commdlg.h>

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
    if (!m_window->Initialize(1600, 900, "Realtime Rendering Engine", hInstance))
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

    // Build empty scene graph (no initial objects — load via File > Open Scene)
    m_sceneGraph = std::make_unique<SceneGraph>();

    // Create renderer
    m_renderer = std::make_unique<Renderer>();
    {
        auto* d3dDevice = static_cast<D3D12Device*>(m_rhiDevice.get());
        auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());
        m_renderer->SetContext(context, d3dDevice->GetD3DDevice());

        // Create texture cache and connect to renderer
        // Open command list for fallback texture upload
        context->BeginUploadCommands();
        m_textureCache = std::make_unique<TextureCache>();
        m_textureCache->Initialize(d3dDevice->GetD3DDevice(),
                                   context->GetCommandList(),
                                   &context->GetCBVSRVHeap());
        context->EndUploadCommands();
        m_textureCache->ReleaseUploadBuffers();
        m_renderer->SetTextureCache(m_textureCache.get());

        // Initialize texture streamer (VRAM monitoring + priority tracking)
        m_textureStreamer = std::make_unique<TextureStreamer>();
        m_textureStreamer->Initialize(d3dDevice->GetDXGIAdapter3());
    }

    // Create worker thread pool for async texture decoding / resource loading
    // Use hardware_concurrency() workers (defaults to 0 → auto-detect in ThreadPool ctor)
    m_threadPool = std::make_unique<ThreadPool>();

    // Create menu
    m_menu = std::make_unique<Win32Menu>();
    m_menu->Initialize(m_window->GetHWND());
    m_menu->SetViewCallback([this](uint32 w, uint32 h, bool fullscreen) {
        OnViewModeChanged(w, h, fullscreen);
    });
    m_menu->SetFileOpenCallback([this]() {
        ShowOpenSceneDialog();
    });
    m_menu->SetFileSponzaCallback([this]() {
        LoadSponzaScene();
    });
    m_menu->SetFileBistroCallback([this]() {
        LoadBistroScene();
    });
    m_menu->SetCameraFitToSceneCallback([this]() {
        if (m_camera)
        {
            BoundingBox bounds;
            m_sceneGraph->Traverse([&bounds](SceneNode* node, const XMMATRIX& world) {
                if (node->GetMesh())
                {
                    for (const auto& v : node->GetMesh()->vertices)
                    {
                        XMVECTOR pos = XMLoadFloat3(&v.position);
                        pos = XMVector3Transform(pos, world);
                        XMFLOAT3 wp;
                        XMStoreFloat3(&wp, pos);
                        bounds.Expand(wp);
                    }
                }
            });
            if (bounds.IsValid())
            {
                m_sceneDiagonal = bounds.GetDiagonalLength();
                m_camera->FitToScene(bounds.GetCenter(), m_sceneDiagonal);
                m_camera->SetMoveSpeedScale(m_sceneDiagonal / 10.0f);
            }
        }
    });
    m_window->SetMenu(m_menu.get());


    // Mouse callbacks
    m_window->SetRightDragCallback([this](int dx, int dy) {
        if (m_camera)
        {
            constexpr float sensitivity = 0.003f;
            m_camera->Rotate(dx * sensitivity, -dy * sensitivity);
        }
    });
    m_window->SetMouseWheelCallback([this](int delta) {
        if (m_camera)
        {
            float wheelSpeed = 0.5f;
            m_camera->MoveForward(delta / 120.0f * wheelSpeed);
        }
    });
    m_window->SetMiddleDragCallback([this](int dx, int dy) {
        if (m_camera)
        {
            constexpr float panSpeed = 0.01f;
            m_camera->MoveRight(-dx * panSpeed);
            m_camera->MoveUp(dy * panSpeed);
        }
    });
    m_window->SetDropFileCallback([this](const std::string& filePath) {
        LoadScene(filePath);
    });

    // Create light manager (lights are set up when a scene is loaded)
    m_lightManager = std::make_unique<LightManager>();

    // Set light menu callbacks
    m_menu->SetLightColorCallback([this](float r, float g, float b) {
        if (m_lightManager)
        {
            for (size_t i = 0; i < m_lightManager->GetActiveLightCount(); i++)
                m_lightManager->GetLightMutable(i).color = { r, g, b };
        }
    });
    m_menu->SetLightToggleInfoCallback([this]() {
        m_showLightInfo = !m_showLightInfo;
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

    m_menu->SetRenderModeCallback([this](uint32 mode) {
        if (m_renderer)
            m_renderer->SetRenderMode(static_cast<RenderMode>(mode));
    });
    m_menu->SetFrustumCullToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetFrustumCullingEnabled(!m_renderer->IsFrustumCullingEnabled());
    });
    m_menu->SetLightCullToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetLightCullingEnabled(!m_renderer->IsLightCullingEnabled());
    });
    m_menu->SetLODToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetLODEnabled(!m_renderer->IsLODEnabled());
    });
    m_menu->SetMipMapToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetMipMappingEnabled(!m_renderer->IsMipMappingEnabled());
    });
    m_menu->SetOcclusionCullToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetOcclusionCullingEnabled(!m_renderer->IsOcclusionCullingEnabled());
    });
    m_menu->SetCSMToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetCSMEnabled(!m_renderer->IsCSMEnabled());
    });
    m_menu->SetCSMDebugToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetCSMDebugView(!m_renderer->IsCSMDebugView());
    });
    m_menu->SetPCSSToggleCallback([this]() {
        if (m_renderer)
            m_renderer->SetPCSSEnabled(!m_renderer->IsPCSSEnabled());
    });
    m_menu->SetPCSSLightSizeCallback([this](float multiplier) {
        if (m_renderer)
            m_renderer->SetLightSizeMultiplier(multiplier);
    });
    m_menu->SetTorchShadowCountCallback([this](int count) {
        if (!m_lightManager || !m_isSponzaScene) return;
        for (int i = 0; i < 4; i++)
        {
            size_t idx = m_sponzaTorchStartIndex + i;
            if (idx < m_lightManager->GetActiveLightCount())
                m_lightManager->GetLightMutable(idx).castShadow = (i < count);
        }
    });

    // Create animation controller (Phase 34 Part A)
    m_animController = std::make_unique<AnimationController>();
    m_menu->SetAnimationPlayPauseCallback([this]() {
        if (!m_animController) return;
        if (m_animController->IsPlaying())
            m_animController->Pause();
        else
            m_animController->Play();
    });
    m_menu->SetAnimationStopCallback([this]() {
        if (m_animController) m_animController->Stop();
    });
    m_menu->SetAnimationSpeedCallback([this](float speed) {
        if (m_animController) m_animController->SetPlaybackSpeed(speed);
    });
    m_menu->SetAnimationClipSelectCallback([this](size_t index) {
        if (!m_animController || index >= m_animationClips.size()) return;
        m_activeClipIndex = index;
        m_animController->SetClipByIndex(m_animationClips, index);
        m_animController->Play();
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

    m_textureStreamer.reset();
    m_textureCache.reset();
    m_renderer.reset();
    m_loadedMeshes.clear();
    m_loadedMaterials.clear();
    m_sceneGraph.reset();
    m_lightManager.reset();
    m_camera.reset();
    m_menu.reset();
    m_rhiDevice.reset();
    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // Update orbiting Directional light direction (always active)
    if (m_camera && m_lightManager && m_orbitLightIndex < m_lightManager->GetActiveLightCount())
    {
        m_orbitLightAngle += 0.8f * deltaTime;

        // Orbit around World Y-axis (camera-independent).
        // Light source position on a sphere at 45° elevation:
        //   pos = R * { cosElev*cos(a),  sinElev,  cosElev*sin(a) }
        // Direction = normalize(origin - pos) = { -cosElev*cos(a), -sinElev, -cosElev*sin(a) }
        // Length = sqrt(cosElev^2 + sinElev^2) = 1  (already unit vector)
        constexpr float kElevRad = XM_PI / 4.0f;   // 45° below horizontal
        const float cosElev = cosf(kElevRad);       // ~0.7071
        const float sinElev = sinf(kElevRad);       // ~0.7071
        XMFLOAT3 lightDir = {
            -cosElev * cosf(m_orbitLightAngle),
            -sinElev,
            -cosElev * sinf(m_orbitLightAngle)
        };
        m_lightManager->GetLightMutable(m_orbitLightIndex).direction = lightDir;
    }

    if (m_window->HasFocus())
    {
        // Sponza sun direction toggle (L key) — only when loaded via Sponza! menu
        if (m_isSponzaScene && m_lightManager &&
            m_sponzaSunKeyIndex < m_lightManager->GetActiveLightCount())
        {
            bool keyDown = (GetAsyncKeyState('L') & 0x8000) != 0;
            if (keyDown && !m_sponzaSunToggleKeyWasDown)
            {
                m_sponzaSunAltMode = !m_sponzaSunAltMode;
                XMVECTOR dir = m_sponzaSunAltMode
                    ? XMVector3Normalize(XMVectorSet(-0.3f, -1.5f, 0.3f, 0.0f))
                    : XMVector3Normalize(XMVectorSet(-0.3f, -1.0f, 0.5f, 0.0f));
                XMStoreFloat3(&m_lightManager->GetLightMutable(m_sponzaSunKeyIndex).direction, dir);
            }
            m_sponzaSunToggleKeyWasDown = keyDown;
        }

        // Bistro interior/exterior lighting toggle (L key) — only when loaded via Bistro! menu
        if (m_isBistroScene && m_lightManager)
        {
            bool keyDown = (GetAsyncKeyState('L') & 0x8000) != 0;
            if (keyDown && !m_bistroLightKeyWasDown)
            {
                m_bistroInteriorMode = !m_bistroInteriorMode;
                ApplyBistroLighting();
            }
            m_bistroLightKeyWasDown = keyDown;
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
    }

    // Update animation (Phase 34 Part A)
    if (m_animController)
        m_animController->Update(deltaTime);

    // Update texture streamer: refresh VRAM stats every 0.5 s
    if (m_textureStreamer)
    {
        m_vramUpdateTimer += deltaTime;
        if (m_vramUpdateTimer >= 0.5f)
        {
            m_textureStreamer->UpdateVRAM();
            m_vramUpdateTimer = 0.0f;
        }
    }

    // Update debug HUD
    if (m_debugHUD)
    {
        RenderStats stats = {};
        stats.fps = 0.0f; // DebugHUD calculates this internally
        stats.width = m_window->GetWidth();
        stats.height = m_window->GetHeight();
        stats.aspectRatio = static_cast<float>(stats.width) / static_cast<float>(stats.height);
        // Full scene polygon count (before any culling or LOD)
        stats.totalPolygons = m_sceneGraph ? m_sceneGraph->GetTotalPolygonCount() : 0;

        // Actual rendered polygon count (after frustum culling + LOD selection)
        if (m_renderer)
            stats.renderedPolygons = m_renderer->GetLastCullStats().renderedPolygons;

        stats.polygonsPerSec = stats.renderedPolygons * (1.0f / deltaTime);

        // Count SceneNodes and mesh nodes
        if (m_sceneGraph)
        {
            m_sceneGraph->Traverse([&stats](SceneNode* node, const DirectX::XMMATRIX&) {
                stats.totalNodes++;
                if (node->GetMesh()) stats.totalMeshNodes++;
            });
        }
        stats.showLightInfo = m_showLightInfo;
        if (m_lightManager && m_lightManager->GetActiveLightCount() > 0)
        {
            const Light& firstLight = m_lightManager->GetLight(0);
            stats.lightPosition = firstLight.position;
            // Derive color name from first light's color
            XMFLOAT3 c = firstLight.color;
            if (c.x > 0.9f && c.y > 0.9f && c.z > 0.9f) stats.lightColorName = "White";
            else if (c.x > 0.9f && c.y < 0.1f && c.z < 0.1f) stats.lightColorName = "Red";
            else if (c.x < 0.1f && c.y > 0.9f && c.z < 0.1f) stats.lightColorName = "Green";
            else if (c.x < 0.1f && c.y < 0.1f && c.z > 0.9f) stats.lightColorName = "Blue";
            else if (c.x > 0.9f && c.y > 0.9f && c.z < 0.1f) stats.lightColorName = "Yellow";
            else if (c.x < 0.1f && c.y > 0.9f && c.z > 0.9f) stats.lightColorName = "Cyan";
            else if (c.x > 0.9f && c.y < 0.1f && c.z > 0.9f) stats.lightColorName = "Magenta";
            else stats.lightColorName = "Custom";
        }
        stats.showCameraInfo = m_showCameraInfo;
        if (m_camera)
        {
            stats.projectionModeName = m_camera->GetProjectionModeName();
            stats.cameraPosition = m_camera->GetPosition();
            stats.cameraDirection = m_camera->GetDirection();
            stats.fovDegrees = m_camera->GetFovDegrees();
        }

        // Render mode name
        if (m_renderer)
        {
            static const char* modeNames[] = {
                "Wireframe", "Solid", "Base Color Only", "Full PBR", "Full PBR + Shadows"
            };
            int modeIdx = static_cast<int>(m_renderer->GetRenderMode());
            if (modeIdx >= 0 && modeIdx <= 4)
                stats.renderModeName = modeNames[modeIdx];

            stats.shadowModeName    = m_renderer->IsPCSSEnabled() ? "PCSS" : "PCF";
            stats.cubeShadowPasses  = m_lightManager
                ? m_lightManager->GetPointShadowCasterCount() * 6 : 0;

            // Culling / LOD statistics (Phase 23)
            CullStats cs = m_renderer->GetLastCullStats();
            stats.visibleNodes          = cs.visibleNodes;
            stats.frustumCulledNodes    = cs.frustumCulledNodes;
            stats.occlusionCulledNodes  = cs.occlusionCulledNodes;
            stats.activeLights          = cs.activeLights;
            stats.culledLights          = cs.culledLights;
        }

        // Texture streaming stats + VRAM pressure (Phase 27/29)
        if (m_textureStreamer)
        {
            auto ss = m_textureStreamer->GetStats();
            stats.vramUsedMB      = ss.vram.usedMB;
            stats.vramBudgetMB    = ss.vram.budgetMB;
            stats.trackedTextures = ss.trackedTextures;
            stats.vramPressure    = m_textureStreamer->IsVRAMOverBudget();

            // Propagate VRAM pressure to renderer (adaptive LOD scaling)
            if (m_renderer)
                m_renderer->SetVRAMPressure(stats.vramPressure);
        }

        // Instancing stats (Phase 29)
        if (m_renderer)
        {
            CullStats cs = m_renderer->GetLastCullStats();
            stats.opaqueBatches  = cs.opaqueBatches;
            stats.totalInstances = cs.totalInstances;
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
    m_renderer->RenderScene(*m_sceneGraph, *m_camera, aspectRatio, m_lightManager.get());

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

void Engine::ShowOpenSceneDialog()
{
    wchar_t filePath[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_window->GetHWND();
    ofn.lpstrFilter = L"3D Scene Files (*.gltf;*.glb;*.fbx)\0*.gltf;*.glb;*.fbx\0All Files\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn))
    {
        int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Path(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, filePath, -1, utf8Path.data(), len, nullptr, nullptr);
        LoadScene(utf8Path);
    }
}

void Engine::LoadSponzaScene()
{
    // Show file open dialog (same as Open Scene), then apply Sponza-specific settings
    wchar_t filePath[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_window->GetHWND();
    ofn.lpstrFilter = L"3D Scene Files (*.gltf;*.glb;*.fbx)\0*.gltf;*.glb;*.fbx\0All Files\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return;     // user cancelled

    int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Path(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, utf8Path.data(), len, nullptr, nullptr);

    // Standard load: scene graph, textures, bounds, shadow maps
    LoadScene(utf8Path);

    // --- Camera: Sponza Option B (SceneSettings.md) ---
    if (m_camera)
    {
        m_camera->SetPosition({ 10.0f, 4.5f, 4.0f });
        m_camera->SetLookAt ({ 0.0f,  0.0f, 0.0f });
        m_camera->SetFov(XMConvertToRadians(60.0f));
        m_camera->SetMoveSpeedScale(m_sceneDiagonal / 40.0f);  // quarter of default (diagonal/10)
        // Sponza diagonal ~36 m; set far plane to 10× diagonal (min 500 m)
        float farPlane = m_sceneDiagonal * 10.0f;
        if (farPlane < 500.0f) farPlane = 500.0f;
        m_camera->SetFarPlane(farPlane);
    }

    // --- Lights: replace the generic 3-point setup with Sponza-specific layout ---
    if (m_lightManager)
    {
        m_lightManager->Clear();
        m_orbitLightIndex = SIZE_MAX;   // disable orbit light for this scene

        // Sponza scene flag + sun toggle state
        m_isSponzaScene = true;
        m_sponzaSunAltMode = false;
        m_sponzaSunKeyIndex = 0;        // Key light is added first (index 0)
        m_sponzaTorchStartIndex = 2;    // Key(0), Fill(1), Torch(2~5)

        // Key Light — Directional (sun): warm, casts shadows
        {
            Light key;
            key.type = LightType::Directional;
            XMStoreFloat3(&key.direction,
                XMVector3Normalize(XMVectorSet(-0.3f, -1.0f, 0.5f, 0.0f)));
            key.color      = { 1.0f, 0.95f, 0.8f };
            key.intensity  = 10.0f;
            key.castShadow = true;
            m_lightManager->AddLight(key);
        }

        // Fill Light — Point (sky indirect)
        {
            Light fill;
            fill.type      = LightType::Point;
            fill.position  = { -6.0f, 10.0f, 0.0f };
            fill.color     = { 0.4f, 0.5f, 0.7f };
            fill.intensity = 1.75f;
            fill.Kc = 1.0f; fill.Kl = 0.027f; fill.Kq = 0.005f;
            fill.castShadow = false;
            m_lightManager->AddLight(fill);
        }

        // Torch Point Lights x4 (orange flame, ~3-4 m radius)
        static const XMFLOAT3 kTorchPositions[4] = {
            { +3.901f, 1.836f, +1.765f },   // right-front
            { -4.954f, 1.836f, +1.765f },   // left-front
            { -4.954f, 1.836f, -1.154f },   // left-back
            { +3.901f, 1.836f, -1.154f },   // right-back
        };
        for (const auto& pos : kTorchPositions)
        {
            Light torch;
            torch.type      = LightType::Point;
            torch.position  = pos;
            torch.color     = { 1.0f, 0.45f, 0.08f };
            torch.intensity = 8.0f;
            torch.Kc = 1.0f; torch.Kl = 0.7f; torch.Kq = 1.8f;
            torch.castShadow = false;
            m_lightManager->AddLight(torch);
        }

        if (m_menu) m_menu->SetTorchShadowMenuEnabled(true, 0);
    }
}

void Engine::LoadBistroScene()
{
    // Show file open dialog
    wchar_t filePath[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_window->GetHWND();
    ofn.lpstrFilter = L"3D Scene Files (*.gltf;*.glb;*.fbx)\0*.gltf;*.glb;*.fbx\0All Files\0*.*\0";
    ofn.lpstrFile   = filePath;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return;  // user cancelled

    int len = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Path(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, filePath, -1, utf8Path.data(), len, nullptr, nullptr);

    // Standard load: scene graph, textures, bounds, shadow maps
    LoadScene(utf8Path);

    // --- Camera: Bistro exterior starting view (SceneSettings.md) ---
    if (m_camera)
    {
        m_camera->SetPosition({ 0.0f, 3.0f, -15.0f });
        m_camera->SetLookAt ({ 0.0f, 2.0f,   0.0f });
        m_camera->SetFov(XMConvertToRadians(60.0f));
        m_camera->SetMoveSpeedScale(m_sceneDiagonal / 20.0f);
        // Bistro diagonal ~200 m; set far plane to 10× diagonal (min 500 m)
        float farPlane = m_sceneDiagonal * 10.0f;
        if (farPlane < 500.0f) farPlane = 500.0f;
        m_camera->SetFarPlane(farPlane);
    }

    // Bistro scene flags
    m_isBistroScene = true;
    m_bistroInteriorMode = false;
    m_bistroLightKeyWasDown = false;
    m_orbitLightIndex = SIZE_MAX;   // disable generic orbit light for this scene

    // Apply Bistro exterior lighting
    ApplyBistroLighting();
}

void Engine::ApplyBistroLighting()
{
    if (!m_lightManager)
        return;

    m_lightManager->Clear();

    if (!m_bistroInteriorMode)
    {
        // --- Exterior mode: Evening Sun + Sky ambient fill (2 lights) ---

        // Directional "Evening Sun"
        {
            Light sun;
            sun.type = LightType::Directional;
            XMStoreFloat3(&sun.direction,
                XMVector3Normalize(XMVectorSet(-0.5f, -0.8f, 0.3f, 0.0f)));
            sun.color      = { 1.0f, 0.85f, 0.6f };
            sun.intensity  = 8.0f;
            sun.castShadow = true;
            m_lightManager->AddLight(sun);
        }

        // Point fill (sky ambient)
        {
            Light fill;
            fill.type      = LightType::Point;
            fill.position  = { 0.0f, 20.0f, 0.0f };
            fill.color     = { 0.4f, 0.5f, 0.7f };
            fill.intensity = 2.0f;
            fill.Kc = 1.0f; fill.Kl = 0.007f; fill.Kq = 0.0002f;
            fill.castShadow = false;
            m_lightManager->AddLight(fill);
        }
    }
    else
    {
        // --- Interior mode: dim sun + 3 pendants + bar + ambient (6 lights) ---

        // Directional "Evening Sun" (attenuated — window indirect light)
        {
            Light sun;
            sun.type = LightType::Directional;
            XMStoreFloat3(&sun.direction,
                XMVector3Normalize(XMVectorSet(-0.5f, -0.8f, 0.3f, 0.0f)));
            sun.color      = { 1.0f, 0.85f, 0.6f };
            sun.intensity  = 3.0f;
            sun.castShadow = true;
            m_lightManager->AddLight(sun);
        }

        // Pendant ceiling lights x3 (warm incandescent ~2700K, ~4m radius)
        static const XMFLOAT3 kPendantPositions[3] = {
            { -2.0f, 3.5f,  3.0f },
            { -2.0f, 3.5f,  7.0f },
            { -2.0f, 3.5f, 11.0f },
        };
        for (const auto& pos : kPendantPositions)
        {
            Light pendant;
            pendant.type      = LightType::Point;
            pendant.position  = pos;
            pendant.color     = { 1.0f, 0.85f, 0.55f };
            pendant.intensity = 5.0f;
            pendant.Kc = 1.0f; pendant.Kl = 0.7f; pendant.Kq = 0.9f;
            pendant.castShadow = false;
            m_lightManager->AddLight(pendant);
        }

        // Bar counter light (~2200K amber, ~6m radius)
        {
            Light bar;
            bar.type      = LightType::Point;
            bar.position  = { 2.0f, 3.0f, 5.0f };
            bar.color     = { 1.0f, 0.75f, 0.40f };
            bar.intensity = 4.0f;
            bar.Kc = 1.0f; bar.Kl = 0.35f; bar.Kq = 0.12f;
            bar.castShadow = false;
            m_lightManager->AddLight(bar);
        }

        // Ambient fill (cool sky bounce through openings, ~30m radius)
        {
            Light ambient;
            ambient.type      = LightType::Point;
            ambient.position  = { 0.0f, 8.0f, 6.0f };
            ambient.color     = { 0.7f, 0.65f, 0.90f };
            ambient.intensity = 0.8f;
            ambient.Kc = 1.0f; ambient.Kl = 0.022f; ambient.Kq = 0.0019f;
            ambient.castShadow = false;
            m_lightManager->AddLight(ambient);
        }
    }
}

void Engine::LoadScene(const std::string& filePath)
{
    // Clear scene-specific state (overridden by LoadSponzaScene/LoadBistroScene if called from there)
    m_isSponzaScene = false;
    m_isBistroScene = false;
    if (m_menu) m_menu->SetTorchShadowMenuEnabled(false);

    // Wait for GPU to finish all pending work
    auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());
    context->WaitForGPU();

    // Clear previous scene resources
    if (m_animController) m_animController->SetClip(nullptr);
    m_animationClips.clear();
    m_activeClipIndex = SIZE_MAX;
    if (m_renderer) m_renderer->ClearMeshCache();
    if (m_textureCache) m_textureCache->Clear();
    if (m_textureStreamer) m_textureStreamer->Clear();
    m_loadedMeshes.clear();
    m_loadedMaterials.clear();

    // Load scene via Assimp
    SceneLoader loader;
    SceneData data = loader.LoadScene(filePath);

    if (!data.rootNode)
        return;

    // Store loaded data
    m_loadedMeshes = std::move(data.meshes);
    m_loadedMaterials = std::move(data.materials);

    // Load textures for each material
    // Command list must be in recording state for GPU texture uploads
    if (m_textureCache && !m_loadedMaterials.empty())
    {
        auto* device = static_cast<D3D12Device*>(m_rhiDevice.get())->GetD3DDevice();

        // Open command list for texture uploads (no CBPool/descriptor reset)
        context->BeginUploadCommands();
        auto* cmdList = context->GetCommandList();

        // Helper: load texture from file or embedded data
        auto loadTexture = [&](const std::string& path, bool isSRGB) -> Texture* {
            if (path.empty()) return nullptr;
            if (path[0] == '*')
            {
                auto it = data.embeddedTextures.find(path);
                if (it != data.embeddedTextures.end())
                {
                    const auto& emb = it->second;
                    if (emb.isCompressed)
                    {
                        return m_textureCache->GetOrLoadFromMemory(
                            path, emb.data.data(), emb.data.size(), isSRGB, device, cmdList);
                    }
                    else
                    {
                        return m_textureCache->GetOrLoadFromRawPixels(
                            path, emb.data.data(), emb.width, emb.height, isSRGB, device, cmdList);
                    }
                }
                return nullptr;
            }
            return m_textureCache->GetOrLoad(path, isSRGB, device, cmdList);
        };

        for (auto& mat : m_loadedMaterials)
        {
            mat->baseColorTexture = loadTexture(mat->baseColorTexturePath, true);
            mat->normalTexture = loadTexture(mat->normalTexturePath, false);
            mat->metallicRoughnessTexture = loadTexture(mat->metallicRoughnessTexturePath, false);
            mat->emissiveTexture = loadTexture(mat->emissiveTexturePath, true);
            mat->occlusionTexture = loadTexture(mat->occlusionTexturePath, false);
        }

        // Execute upload commands and wait for completion
        context->EndUploadCommands();

        // Release upload buffers now that GPU copy is complete
        m_textureCache->ReleaseUploadBuffers();
    }

    // Replace scene graph root
    m_sceneGraph->SetRoot(std::move(data.rootNode));

    // Compute world-space scene bounds by applying each node's world transform to its mesh AABB.
    // data.sceneBounds is computed from raw local-space vertices (no node transforms applied),
    // which is incorrect for scenes whose node hierarchy has non-identity transforms (e.g. Sponza).
    BoundingBox worldBounds;
    m_sceneGraph->Traverse([&worldBounds](SceneNode* node, const XMMATRIX& world) {
        Mesh* mesh = node->GetMesh();
        if (mesh && !mesh->vertices.empty())
        {
            DirectX::BoundingBox worldAabb;
            mesh->aabb.Transform(worldAabb, world);
            XMFLOAT3 corners[8];
            worldAabb.GetCorners(corners);
            for (const auto& c : corners)
                worldBounds.Expand(c);
        }
    });
    if (!worldBounds.IsValid())
        worldBounds = data.sceneBounds;  // fallback: local-space bounds if graph yields nothing

    // Camera placement
    if (data.camera.has_value())
    {
        m_camera->SetPosition(data.camera->position);
        m_camera->SetLookAt(data.camera->lookAt);
        m_camera->SetFov(data.camera->fovY);
        m_camera->SetNearPlane(data.camera->nearPlane);
        m_camera->SetFarPlane(data.camera->farPlane);
    }
    else if (worldBounds.IsValid())
    {
        m_sceneDiagonal = worldBounds.GetDiagonalLength();
        m_camera->FitToScene(worldBounds.GetCenter(), m_sceneDiagonal);
    }

    // Adjust movement speed to scene size + setup 3-point lighting
    if (worldBounds.IsValid())
    {
        m_sceneDiagonal = worldBounds.GetDiagonalLength();
        m_camera->SetMoveSpeedScale(m_sceneDiagonal / 10.0f);

        // Setup 3-point lighting relative to scene bounds
        DirectX::XMFLOAT3 center = worldBounds.GetCenter();
        float radius = m_sceneDiagonal * 0.5f;
        float Kl = 0.027f / (m_sceneDiagonal * 0.1f + 1.0f);
        float Kq = 0.005f / (m_sceneDiagonal * 0.1f + 1.0f);

        m_lightManager->Clear();

        // Key Light: warm, bright, upper-right-front
        Light keyLight;
        keyLight.type = LightType::Point;
        keyLight.position = { center.x + 0.5f * radius, center.y + 0.7f * radius, center.z - 0.5f * radius };
        keyLight.color = { 1.0f, 0.95f, 0.9f };
        keyLight.intensity = 8.0f;
        keyLight.Kc = 1.0f; keyLight.Kl = Kl; keyLight.Kq = Kq;
        m_lightManager->AddLight(keyLight);

        // Fill Light: cool, softer, left-side
        Light fillLight;
        fillLight.type = LightType::Point;
        fillLight.position = { center.x - 0.6f * radius, center.y + 0.3f * radius, center.z + 0.4f * radius };
        fillLight.color = { 0.8f, 0.85f, 1.0f };
        fillLight.intensity = 3.0f;
        fillLight.Kc = 1.0f; fillLight.Kl = Kl; fillLight.Kq = Kq;
        m_lightManager->AddLight(fillLight);

        // Back Light: neutral rim light
        Light backLight;
        backLight.type = LightType::Point;
        backLight.position = { center.x, center.y + 0.8f * radius, center.z + 0.6f * radius };
        backLight.color = { 1.0f, 1.0f, 1.0f };
        backLight.intensity = 4.0f;
        backLight.Kc = 1.0f; backLight.Kl = Kl; backLight.Kq = Kq;
        m_lightManager->AddLight(backLight);

        // Orbiting Directional light: casts shadows, direction orbits around camera view axis
        Light orbitLight;
        orbitLight.type       = LightType::Directional;
        orbitLight.direction  = { 0.0f, -1.0f, 0.0f };  // initial; updated every frame in Update()
        orbitLight.color      = { 1.0f, 1.0f, 1.0f };
        orbitLight.intensity  = 6.0f;
        orbitLight.castShadow = true;
        m_orbitLightIndex = m_lightManager->AddLight(orbitLight);
    }

    // Register loaded meshes for async auto-LOD generation.
    // Done after m_sceneDiagonal is finalized so switch distances scale correctly.
    if (m_renderer && !m_loadedMeshes.empty())
        m_renderer->RegisterMeshesForLOD(m_loadedMeshes, m_sceneDiagonal);

    // Notify renderer of scene diagonal and center (shadow ortho projection).
    if (m_renderer)
    {
        m_renderer->SetSceneDiagonal(m_sceneDiagonal);
        if (worldBounds.IsValid())
            m_renderer->SetSceneCenter(worldBounds.GetCenter());
    }

    // Register loaded textures with the streamer for VRAM tracking + priority scoring
    if (m_textureStreamer && m_textureCache)
    {
        m_textureCache->ForEachCached([this](const std::string& key, const Texture*) {
            m_textureStreamer->Register(key);
        });
        m_textureStreamer->UpdateVRAM();
    }

    // Select shadow map resolution based on scene size and recreate shadow maps.
    // Small scenes (<= 10 m diagonal): 1024,  medium (<= 100 m): 2048,  large: 4096.
    {
        uint32 shadowSize = 1024;
        if (m_sceneDiagonal > 100.0f)
            shadowSize = 4096;
        else if (m_sceneDiagonal > 10.0f)
            shadowSize = 2048;
        context->SetShadowMapSize(shadowSize);
        context->RecreateShadowMaps();
    }

    // Set up animation clips (Phase 34 Part A)
    m_animationClips = std::move(data.animations);
    if (m_menu)
    {
        std::vector<std::string> clipNames;
        clipNames.reserve(m_animationClips.size());
        for (const auto& clip : m_animationClips)
            clipNames.push_back(clip.name);
        m_menu->SetAnimationClips(clipNames);
    }
    if (!m_animationClips.empty() && m_animController)
    {
        m_activeClipIndex = 0;
        m_animController->SetClipByIndex(m_animationClips, 0);
        m_animController->Play();
    }

}

} // namespace RRE
