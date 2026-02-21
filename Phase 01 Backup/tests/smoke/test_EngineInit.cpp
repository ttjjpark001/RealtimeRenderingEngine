#include <gtest/gtest.h>
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/D3D12/D3D12Buffer.h"
#include "Renderer/Renderer.h"
#include "Renderer/Mesh.h"
#include "Renderer/MeshFactory.h"
#include "Renderer/Vertex.h"
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Scene/Camera.h"
#include "Lighting/PointLight.h"
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
    auto cubeMesh = std::make_unique<RRE::Mesh>(RRE::MeshFactory::CreateCube());

    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->SetMesh(cubeMesh.get());
    sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(cubeMesh.get());
    childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
    sceneGraph.GetRoot()->GetChildren()[0]->AddChild(std::move(childNode));

    EXPECT_GT(sceneGraph.GetTotalPolygonCount(), 0u);

    // Create Renderer
    RRE::Renderer renderer;
    renderer.SetContext(context, device.GetD3DDevice());

    // Create camera and light
    RRE::Camera camera;
    RRE::PointLight light;

    // Run one render cycle without crashing
    context->BeginFrame();
    DirectX::XMFLOAT4 clearColor(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(clearColor);

    float aspectRatio = 320.0f / 240.0f;
    renderer.RenderScene(sceneGraph, camera, &light, aspectRatio);

    context->EndFrame();

    // Cleanup
    renderer.ClearMeshCache();
    device.Shutdown();
    DestroyWindow(hwnd);
}

TEST(EngineInit, MeshTypeChangeUpdatesSceneNodes)
{
    RRE::SceneGraph sceneGraph;
    auto cubeMesh = std::make_unique<RRE::Mesh>(RRE::MeshFactory::CreateCube());
    auto sphereMesh = std::make_unique<RRE::Mesh>(RRE::MeshFactory::CreateSphere());

    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->SetMesh(cubeMesh.get());
    auto* parent = sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(cubeMesh.get());
    childNode->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
    auto* child = parent->AddChild(std::move(childNode));

    // Both should point to cube
    EXPECT_EQ(parent->GetMesh(), cubeMesh.get());
    EXPECT_EQ(child->GetMesh(), cubeMesh.get());

    // Switch both to sphere (simulates OnMeshTypeChanged)
    parent->SetMesh(sphereMesh.get());
    child->SetMesh(sphereMesh.get());

    EXPECT_EQ(parent->GetMesh(), sphereMesh.get());
    EXPECT_EQ(child->GetMesh(), sphereMesh.get());
}

TEST(EngineInit, ParentRotationAffectsChildWorldMatrix)
{
    RRE::SceneGraph sceneGraph;
    auto cubeMesh = std::make_unique<RRE::Mesh>(RRE::MeshFactory::CreateCube());

    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->SetMesh(cubeMesh.get());
    auto* parent = sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(cubeMesh.get());
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
