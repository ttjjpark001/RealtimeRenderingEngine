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

    // Create all 4 mesh types
    m_sphereMesh = std::make_unique<Mesh>(MeshFactory::CreateSphere());
    m_tetrahedronMesh = std::make_unique<Mesh>(MeshFactory::CreateTetrahedron());
    m_cubeMesh = std::make_unique<Mesh>(MeshFactory::CreateCube());
    m_cylinderMesh = std::make_unique<Mesh>(MeshFactory::CreateCylinder());
    m_currentMesh = m_cubeMesh.get();

    // Build scene graph:
    //   Root -> Parent (self-rotation)
    //   Root -> OrbitPivot (orbital rotation, no mesh) -> Child (offset + self-rotation)
    m_sceneGraph = std::make_unique<SceneGraph>();
    {
        auto parentNode = std::make_unique<SceneNode>();
        parentNode->SetMesh(m_currentMesh);
        m_parentNode = m_sceneGraph->GetRoot()->AddChild(std::move(parentNode));

        auto orbitPivot = std::make_unique<SceneNode>();  // no mesh, controls orbit speed
        m_orbitPivotNode = m_sceneGraph->GetRoot()->AddChild(std::move(orbitPivot));

        auto childNode = std::make_unique<SceneNode>();
        childNode->SetMesh(m_currentMesh);
        childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
        m_childNode = m_orbitPivotNode->AddChild(std::move(childNode));
    }

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
    m_menu->SetMeshCallback([this](MeshType type) {
        OnMeshTypeChanged(type);
    });
    m_menu->SetAnimCallback([this]() {
        OnAnimationToggle();
    });
    m_menu->SetFileOpenCallback([this]() {
        ShowOpenSceneDialog();
    });
    m_menu->SetCameraFitToSceneCallback([this]() {
        if (m_camera)
        {
            if (m_isExternalScene)
            {
                // Recompute from loaded scene bounds
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
            else
            {
                m_camera->Reset();
            }
        }
    });
    m_window->SetMenu(m_menu.get());

    // Set key callback for Space key animation toggle
    m_window->SetKeyCallback([this](WPARAM key) {
        if (key == VK_SPACE)
            OnAnimationToggle();
    });

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

    // Create light manager with default 3-point lighting
    m_lightManager = std::make_unique<LightManager>();
    {
        // Key Light: warm, bright, upper-right-front
        Light keyLight;
        keyLight.type = LightType::Point;
        keyLight.position = { 2.0f, 2.5f, -2.0f };
        keyLight.color = { 1.0f, 0.95f, 0.9f };
        keyLight.intensity = 8.0f;
        keyLight.Kc = 1.0f; keyLight.Kl = 0.027f; keyLight.Kq = 0.005f;
        m_lightManager->AddLight(keyLight);

        // Fill Light: cool, softer, left-side
        Light fillLight;
        fillLight.type = LightType::Point;
        fillLight.position = { -2.5f, 1.5f, 1.5f };
        fillLight.color = { 0.8f, 0.85f, 1.0f };
        fillLight.intensity = 3.0f;
        fillLight.Kc = 1.0f; fillLight.Kl = 0.027f; fillLight.Kq = 0.005f;
        m_lightManager->AddLight(fillLight);

        // Back Light: neutral rim light
        Light backLight;
        backLight.type = LightType::Point;
        backLight.position = { 0.0f, 3.0f, 2.5f };
        backLight.color = { 1.0f, 1.0f, 1.0f };
        backLight.intensity = 4.0f;
        backLight.Kc = 1.0f; backLight.Kl = 0.027f; backLight.Kq = 0.005f;
        m_lightManager->AddLight(backLight);

        // Orbiting light: orbits around camera view axis for PBR visualization
        Light orbitLight;
        orbitLight.type = LightType::Point;
        orbitLight.position = { 1.0f, 1.0f, -1.0f };
        orbitLight.color = { 1.0f, 1.0f, 1.0f };
        orbitLight.intensity = 6.0f;
        orbitLight.Kc = 1.0f; orbitLight.Kl = 0.027f; orbitLight.Kq = 0.005f;
        m_orbitLightIndex = m_lightManager->AddLight(orbitLight);
    }

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
    m_parentNode = nullptr;
    m_orbitPivotNode = nullptr;
    m_childNode = nullptr;
    m_sceneGraph.reset();
    m_currentMesh = nullptr;
    m_sphereMesh.reset();
    m_tetrahedronMesh.reset();
    m_cubeMesh.reset();
    m_cylinderMesh.reset();
    m_lightManager.reset();
    m_camera.reset();
    m_menu.reset();
    m_rhiDevice.reset();
    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // Animate: parent self-rotation, orbit pivot, child self-rotation (all independent speeds)
    if (m_isAnimating)
    {
        m_rotationAngle += 1.0f * deltaTime;         // parent self-rotation
        m_orbitAngle += 0.6f * deltaTime;             // child orbit (slower than parent spin)
        m_childRotationAngle -= 1.5f * deltaTime;     // child self-rotation (opposite, faster)
    }
    if (m_parentNode)
        m_parentNode->GetTransform().SetRotation({ 0.0f, m_rotationAngle, 0.0f });
    if (m_orbitPivotNode)
        m_orbitPivotNode->GetTransform().SetRotation({ 0.0f, m_orbitAngle, 0.0f });
    if (m_childNode)
        m_childNode->GetTransform().SetRotation({ 0.0f, m_childRotationAngle, 0.0f });

    // Update orbiting light position (always active, independent of animation toggle)
    if (m_camera && m_lightManager && m_orbitLightIndex < m_lightManager->GetActiveLightCount())
    {
        m_orbitLightAngle += 0.8f * deltaTime;

        XMFLOAT3 camPos = m_camera->GetPosition();
        XMFLOAT3 camDir = m_camera->GetDirection();
        XMVECTOR vCamPos = XMLoadFloat3(&camPos);
        XMVECTOR vCamDir = XMLoadFloat3(&camDir);

        XMVECTOR orbitCenter = XMVectorAdd(vCamPos, XMVectorScale(vCamDir, m_sceneDiagonal * 0.3f));
        float orbitRadius = m_sceneDiagonal * 0.45f;

        XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, vCamDir));
        XMVECTOR up = XMVector3Cross(vCamDir, right);

        float cosA = cosf(m_orbitLightAngle);
        float sinA = sinf(m_orbitLightAngle);
        XMVECTOR offset = XMVectorAdd(
            XMVectorScale(right, orbitRadius * cosA),
            XMVectorScale(up, orbitRadius * sinA)
        );

        XMFLOAT3 lightPos;
        XMStoreFloat3(&lightPos, XMVectorAdd(orbitCenter, offset));
        m_lightManager->GetLightMutable(m_orbitLightIndex).position = lightPos;
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
    m_parentNode = nullptr;
    m_orbitPivotNode = nullptr;
    m_childNode = nullptr;
    m_sceneGraph->SetRoot(std::move(data.rootNode));

    // Camera placement
    if (data.camera.has_value())
    {
        m_camera->SetPosition(data.camera->position);
        m_camera->SetLookAt(data.camera->lookAt);
        m_camera->SetFov(data.camera->fovY);
    }
    else if (data.sceneBounds.IsValid())
    {
        m_sceneDiagonal = data.sceneBounds.GetDiagonalLength();
        m_camera->FitToScene(data.sceneBounds.GetCenter(), m_sceneDiagonal);
    }

    // Adjust movement speed to scene size + setup 3-point lighting
    if (data.sceneBounds.IsValid())
    {
        m_sceneDiagonal = data.sceneBounds.GetDiagonalLength();
        m_camera->SetMoveSpeedScale(m_sceneDiagonal / 10.0f);

        // Setup 3-point lighting relative to scene bounds
        DirectX::XMFLOAT3 center = data.sceneBounds.GetCenter();
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

        // Orbiting light: orbits around camera view axis for PBR visualization
        Light orbitLight;
        orbitLight.type = LightType::Point;
        orbitLight.position = center;
        orbitLight.color = { 1.0f, 1.0f, 1.0f };
        orbitLight.intensity = 6.0f;
        orbitLight.Kc = 1.0f; orbitLight.Kl = Kl; orbitLight.Kq = Kq;
        m_orbitLightIndex = m_lightManager->AddLight(orbitLight);
    }

    m_isExternalScene = true;
    m_isAnimating = false;
    if (m_menu) m_menu->UpdateAnimCheckMark(false);
}

} // namespace RRE
