#include <gtest/gtest.h>
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Lighting/Light.h"
#include "Lighting/LightManager.h"
#include <windows.h>

namespace
{

HWND CreateTestWindow()
{
    static bool registered = false;
    static const wchar_t* CLASS_NAME = L"EngineInitTestClass";

    if (!registered)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = CLASS_NAME;
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"EngineInitTest",
        WS_OVERLAPPEDWINDOW, 0, 0, 320, 240,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    return hwnd;
}

// Create a minimal triangle mesh for testing (no MeshFactory dependency)
std::unique_ptr<RRE::Mesh> CreateTestTriangleMesh()
{
    auto mesh = std::make_unique<RRE::Mesh>();
    RRE::Vertex v0{}, v1{}, v2{};
    v0.position = {  0.0f,  1.0f, 0.0f }; v0.color = {1,1,1,1}; v0.normal = {0,0,-1};
    v1.position = { -1.0f, -1.0f, 0.0f }; v1.color = {1,1,1,1}; v1.normal = {0,0,-1};
    v2.position = {  1.0f, -1.0f, 0.0f }; v2.color = {1,1,1,1}; v2.normal = {0,0,-1};
    mesh->vertices = { v0, v1, v2 };
    mesh->indices  = { 0, 1, 2 };
    return mesh;
}

} // anonymous namespace

TEST(EngineInit, SceneGraphWithRendererOneCycle)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    // Initialize WARP device
    RRE::D3D12Device device;
    bool result = device.InitializeWARP(hwnd, 320, 240);
    ASSERT_TRUE(result);

    auto* context = static_cast<RRE::D3D12Context*>(device.GetContext());
    ASSERT_NE(context, nullptr);

    // Build scene graph: root -> parent -> child
    RRE::SceneGraph sceneGraph;
    auto triangleMesh = CreateTestTriangleMesh();

    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->SetMesh(triangleMesh.get());
    sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(triangleMesh.get());
    childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
    sceneGraph.GetRoot()->GetChildren()[0]->AddChild(std::move(childNode));

    EXPECT_GT(sceneGraph.GetTotalPolygonCount(), 0u);

    // Create Renderer
    RRE::Renderer renderer;
    renderer.SetContext(context, device.GetD3DDevice());

    // Create camera and light manager
    RRE::Camera camera;
    RRE::LightManager lightManager;
    RRE::Light defaultLight;
    defaultLight.type = RRE::LightType::Point;
    defaultLight.position = { 2.0f, 3.0f, -2.0f };
    defaultLight.color = { 1.0f, 1.0f, 1.0f };
    defaultLight.intensity = 8.0f;
    lightManager.AddLight(defaultLight);

    // Run one render cycle without crashing
    context->BeginFrame();
    DirectX::XMFLOAT4 clearColor(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(clearColor);

    float aspectRatio = 320.0f / 240.0f;
    renderer.RenderScene(sceneGraph, camera, aspectRatio, &lightManager);

    context->EndFrame();

    // Cleanup
    renderer.ClearMeshCache();
    device.Shutdown();
    DestroyWindow(hwnd);
}

TEST(EngineInit, ParentRotationAffectsChildWorldMatrix)
{
    RRE::SceneGraph sceneGraph;
    auto triangleMesh = CreateTestTriangleMesh();

    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->SetMesh(triangleMesh.get());
    auto* parent = sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(triangleMesh.get());
    childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
    auto* child = parent->AddChild(std::move(childNode));

    // Rotate parent by 90 degrees around Y
    parent->GetTransform().SetRotation({ 0.0f, DirectX::XM_PIDIV2, 0.0f });

    // Child's world position should have changed
    DirectX::XMMATRIX afterWorld = child->GetWorldMatrix();
    DirectX::XMFLOAT4X4 afterMat;
    DirectX::XMStoreFloat4x4(&afterMat, afterWorld);
    float afterX = afterMat._41;
    float afterZ = afterMat._43;

    // After 90-degree Y rotation, child at (3,0,0) should move to approximately (0,0,-3)
    // (local * parent order: translate first, then rotate)
    EXPECT_NEAR(afterX, 0.0f, 0.01f);
    EXPECT_NEAR(afterZ, -3.0f, 0.01f);
}

TEST(EngineInit, PointLightCastShadow_OneCycleDoesNotCrash)
{
    // Verifies that a Point light with castShadow=true goes through one render
    // cycle without crashing. Exercises CreateCubeShadowMaps + 6-pass shadow path.
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    ASSERT_TRUE(device.InitializeWARP(hwnd, 320, 240));

    auto* context = static_cast<RRE::D3D12Context*>(device.GetContext());
    ASSERT_NE(context, nullptr);

    // Scene with one triangle
    RRE::SceneGraph sceneGraph;
    auto mesh = CreateTestTriangleMesh();
    auto node = std::make_unique<RRE::SceneNode>();
    node->SetMesh(mesh.get());
    sceneGraph.GetRoot()->AddChild(std::move(node));

    // Renderer
    RRE::Renderer renderer;
    renderer.SetContext(context, device.GetD3DDevice());
    renderer.SetSceneDiagonal(10.0f);
    renderer.SetSceneCenter({ 0.0f, 0.0f, 0.0f });

    // Point light with castShadow=true
    RRE::LightManager lightManager;
    RRE::Light pointLight;
    pointLight.type       = RRE::LightType::Point;
    pointLight.position   = { 0.0f, 3.0f, 0.0f };
    pointLight.color      = { 1.0f, 1.0f, 1.0f };
    pointLight.intensity  = 10.0f;
    pointLight.castShadow = true;
    lightManager.AddLight(pointLight);

    RRE::Camera camera;

    context->BeginFrame();
    context->Clear({ 0.0f, 0.0f, 0.0f, 1.0f });
    renderer.RenderScene(sceneGraph, camera, 320.0f / 240.0f, &lightManager);
    context->EndFrame();

    // DebugHUD should report 6 cube shadow passes (1 light × 6 faces)
    EXPECT_EQ(lightManager.GetPointShadowCasterCount(), 1u);

    renderer.ClearMeshCache();
    device.Shutdown();
    DestroyWindow(hwnd);
}
