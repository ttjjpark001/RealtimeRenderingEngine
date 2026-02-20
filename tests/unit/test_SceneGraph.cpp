#include <gtest/gtest.h>
#include "Scene/SceneGraph.h"
#include "Scene/SceneNode.h"
#include "Renderer/Mesh.h"
#include "Math/MathUtil.h"

using namespace DirectX;
using namespace RRE;

TEST(SceneGraph, RootNodeExists)
{
    SceneGraph graph;
    EXPECT_NE(graph.GetRoot(), nullptr);
}

TEST(SceneGraph, AddChild)
{
    SceneGraph graph;
    auto child = std::make_unique<SceneNode>();
    SceneNode* childPtr = graph.GetRoot()->AddChild(std::move(child));

    EXPECT_NE(childPtr, nullptr);
    EXPECT_EQ(childPtr->GetParent(), graph.GetRoot());
    EXPECT_EQ(graph.GetRoot()->GetChildren().size(), 1u);
}

TEST(SceneGraph, RemoveChild)
{
    SceneGraph graph;
    auto child = std::make_unique<SceneNode>();
    SceneNode* childPtr = graph.GetRoot()->AddChild(std::move(child));

    auto removed = graph.GetRoot()->RemoveChild(childPtr);
    EXPECT_NE(removed, nullptr);
    EXPECT_EQ(removed->GetParent(), nullptr);
    EXPECT_EQ(graph.GetRoot()->GetChildren().size(), 0u);
}

TEST(SceneGraph, RemoveNonexistentChild)
{
    SceneGraph graph;
    SceneNode dummy;
    auto removed = graph.GetRoot()->RemoveChild(&dummy);
    EXPECT_EQ(removed, nullptr);
}

TEST(SceneGraph, ChildWorldMatrixIncludesParent)
{
    SceneGraph graph;

    // Parent translated to (5, 0, 0)
    graph.GetRoot()->GetTransform().SetPosition({ 5.0f, 0.0f, 0.0f });

    // Child translated to (3, 0, 0) in local space
    auto child = std::make_unique<SceneNode>();
    child->GetTransform().SetPosition({ 3.0f, 0.0f, 0.0f });
    SceneNode* childPtr = graph.GetRoot()->AddChild(std::move(child));

    // Child world matrix should place origin at (8, 0, 0)
    XMMATRIX childWorld = childPtr->GetWorldMatrix();
    XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(origin, childWorld);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    EXPECT_TRUE(Math::NearEqual(pos.x, 8.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, 0.0f));
}

TEST(SceneGraph, ParentRotationAffectsChildPosition)
{
    SceneGraph graph;

    // Parent rotated 90 degrees around Y
    graph.GetRoot()->GetTransform().SetRotation({ 0.0f, XM_PIDIV2, 0.0f });

    // Child at (1, 0, 0) in local space
    auto child = std::make_unique<SceneNode>();
    child->GetTransform().SetPosition({ 1.0f, 0.0f, 0.0f });
    SceneNode* childPtr = graph.GetRoot()->AddChild(std::move(child));

    // After parent rotation: child's world position of (1,0,0) should become (0,0,-1)
    XMMATRIX childWorld = childPtr->GetWorldMatrix();
    XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMVECTOR result = XMVector4Transform(origin, childWorld);
    XMFLOAT3 pos;
    XMStoreFloat3(&pos, result);

    EXPECT_TRUE(Math::NearEqual(pos.x, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.y, 0.0f));
    EXPECT_TRUE(Math::NearEqual(pos.z, -1.0f));
}

TEST(SceneGraph, TraverseVisitsAllNodes)
{
    SceneGraph graph;

    auto child1 = std::make_unique<SceneNode>();
    auto child2 = std::make_unique<SceneNode>();
    auto grandchild = std::make_unique<SceneNode>();

    SceneNode* c1 = graph.GetRoot()->AddChild(std::move(child1));
    graph.GetRoot()->AddChild(std::move(child2));
    c1->AddChild(std::move(grandchild));

    int count = 0;
    graph.Traverse([&count](SceneNode*, const XMMATRIX&) {
        count++;
    });

    // Root + 2 children + 1 grandchild = 4
    EXPECT_EQ(count, 4);
}

TEST(SceneGraph, GetTotalPolygonCount)
{
    SceneGraph graph;

    Mesh mesh;
    mesh.indices.resize(12); // 4 triangles

    auto child = std::make_unique<SceneNode>();
    child->SetMesh(&mesh);
    graph.GetRoot()->AddChild(std::move(child));

    auto child2 = std::make_unique<SceneNode>();
    child2->SetMesh(&mesh);
    graph.GetRoot()->AddChild(std::move(child2));

    // 2 nodes * 4 polygons = 8
    EXPECT_EQ(graph.GetTotalPolygonCount(), 8u);
}

TEST(SceneGraph, GetTotalPolygonCountNoMesh)
{
    SceneGraph graph;
    EXPECT_EQ(graph.GetTotalPolygonCount(), 0u);
}
