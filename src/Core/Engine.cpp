#include "Core/Engine.h"
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
    }

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

            // Culling / LOD statistics (Phase 23)
            CullStats cs = m_renderer->GetLastCullStats();
            stats.visibleNodes       = cs.visibleNodes;
            stats.frustumCulledNodes = cs.frustumCulledNodes;
            stats.activeLights       = cs.activeLights;
            stats.culledLights       = cs.culledLights;
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
    }

    // --- Lights: replace the generic 3-point setup with Sponza-specific layout ---
    if (m_lightManager)
    {
        m_lightManager->Clear();
        m_orbitLightIndex = SIZE_MAX;   // disable orbit light for this scene

        // Key Light — Directional (sun): warm, casts shadows
        {
            Light key;
            key.type = LightType::Directional;
            XMStoreFloat3(&key.direction,
                XMVector3Normalize(XMVectorSet(-0.3f, -1.0f, 0.5f, 0.0f)));
            key.color      = { 1.0f, 0.95f, 0.8f };
            key.intensity  = 7.0f;
            key.castShadow = true;
            m_lightManager->AddLight(key);
        }

        // Fill Light — Point (sky indirect)
        {
            Light fill;
            fill.type      = LightType::Point;
            fill.position  = { -8.0f, 12.0f, 6.0f };
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
            torch.Kc = 1.0f; torch.Kl = 0.35f; torch.Kq = 0.44f;
            torch.castShadow = false;
            m_lightManager->AddLight(torch);
        }
    }
}

void Engine::LoadScene(const std::string& filePath)
{
    // Wait for GPU to finish all pending work
    auto* context = static_cast<D3D12Context*>(m_rhiDevice->GetContext());
    context->WaitForGPU();

    // Clear previous scene resources
    if (m_renderer) m_renderer->ClearMeshCache();
    if (m_textureCache) m_textureCache->Clear();
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

}

} // namespace RRE
