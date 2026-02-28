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
        m_textureCache = std::make_unique<TextureCache>();
        m_textureCache->Initialize(d3dDevice->GetD3DDevice(),
                                   context->GetCommandList(),
                                   &context->GetCBVSRVHeap());
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

    // Create point light
    m_pointLight = std::make_unique<PointLight>();

    // Create light manager and add the point light
    m_lightManager = std::make_unique<LightManager>();
    {
        Light mainLight;
        mainLight.type = LightType::Point;
        mainLight.position = m_pointLight->GetPosition();
        mainLight.color = m_pointLight->GetColor();
        mainLight.intensity = 1.0f;
        mainLight.Kc = m_pointLight->GetConstantAttenuation();
        mainLight.Kl = m_pointLight->GetLinearAttenuation();
        mainLight.Kq = m_pointLight->GetQuadraticAttenuation();
        m_lightManager->AddLight(mainLight);
    }

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
        if (m_lightManager && m_lightManager->GetActiveLightCount() > 0)
            m_lightManager->GetLightMutable(0).color = { r, g, b };
    });
    m_menu->SetLightToggleInfoCallback([this]() {
        m_showLightInfo = !m_showLightInfo;
    });
    m_menu->SetLightResetCallback([this]() {
        m_pointLight->Reset();
        if (m_lightManager && m_lightManager->GetActiveLightCount() > 0)
        {
            Light& light = m_lightManager->GetLightMutable(0);
            light.position = m_pointLight->GetPosition();
            light.color = m_pointLight->GetColor();
        }
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
    m_lightSphereVB.reset();
    m_lightSphereIB.reset();
    m_lightSphereMesh.reset();
    m_pointLight.reset();
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
        if (m_lightManager && m_lightManager->GetActiveLightCount() > 0)
            m_lightManager->GetLightMutable(0).position = pos;
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
    m_renderer->RenderScene(*m_sceneGraph, *m_camera, m_pointLight.get(), aspectRatio,
        m_lightManager.get());

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

        // Open command list for texture upload commands
        context->BeginFrame();
        auto* cmdList = context->GetCommandList();

        for (auto& mat : m_loadedMaterials)
        {
            if (!mat->baseColorTexturePath.empty())
                mat->baseColorTexture = m_textureCache->GetOrLoad(mat->baseColorTexturePath, true, device, cmdList);
            if (!mat->normalTexturePath.empty())
                mat->normalTexture = m_textureCache->GetOrLoad(mat->normalTexturePath, false, device, cmdList);
            if (!mat->metallicRoughnessTexturePath.empty())
                mat->metallicRoughnessTexture = m_textureCache->GetOrLoad(mat->metallicRoughnessTexturePath, false, device, cmdList);
            if (!mat->emissiveTexturePath.empty())
                mat->emissiveTexture = m_textureCache->GetOrLoad(mat->emissiveTexturePath, true, device, cmdList);
            if (!mat->occlusionTexturePath.empty())
                mat->occlusionTexture = m_textureCache->GetOrLoad(mat->occlusionTexturePath, false, device, cmdList);
        }

        // Execute upload commands and wait for completion
        cmdList->Close();
        ID3D12CommandList* cmdLists[] = { cmdList };
        context->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);
        context->WaitForGPU();

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

    // Adjust movement speed to scene size + reposition light
    if (data.sceneBounds.IsValid())
    {
        m_sceneDiagonal = data.sceneBounds.GetDiagonalLength();
        m_camera->SetMoveSpeedScale(m_sceneDiagonal / 10.0f);

        // Reposition point light relative to scene bounds
        DirectX::XMFLOAT3 center = data.sceneBounds.GetCenter();
        float offset = m_sceneDiagonal * 0.5f;
        DirectX::XMFLOAT3 lightPos = { center.x + offset, center.y + offset, center.z - offset };
        if (m_pointLight)
            m_pointLight->SetPosition(lightPos);
        if (m_lightManager && m_lightManager->GetActiveLightCount() > 0)
        {
            m_lightManager->GetLightMutable(0).position = lightPos;
            // Scale attenuation to scene size so light reaches entire scene
            float Kl = 0.09f / (m_sceneDiagonal * 0.1f + 1.0f);
            float Kq = 0.032f / (m_sceneDiagonal * 0.1f + 1.0f);
            m_lightManager->GetLightMutable(0).Kl = Kl;
            m_lightManager->GetLightMutable(0).Kq = Kq;
        }
    }

    m_isExternalScene = true;
    m_isAnimating = false;
    if (m_menu) m_menu->UpdateAnimCheckMark(false);
}

} // namespace RRE
