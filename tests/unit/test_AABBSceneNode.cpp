#include <gtest/gtest.h>
#include "Renderer/Mesh.h"
#include "Renderer/Vertex.h"
#include "Scene/SceneNode.h"
#include "Scene/SceneGraph.h"
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <cmath>

using namespace DirectX;

namespace
{

// Build a mesh with the given min/max extents (axis-aligned box vertices)
std::unique_ptr<RRE::Mesh> MakeMesh(XMFLOAT3 minPt, XMFLOAT3 maxPt)
{
    auto mesh = std::make_unique<RRE::Mesh>();
    // 8 corners of the box
    float xs[] = { minPt.x, maxPt.x };
    float ys[] = { minPt.y, maxPt.y };
    float zs[] = { minPt.z, maxPt.z };
    for (float x : xs) for (float y : ys) for (float z : zs)
    {
        RRE::Vertex v{};
        v.position = { x, y, z };
        v.color = { 1, 1, 1, 1 };
        v.normal = { 0, 1, 0 };
        mesh->vertices.push_back(v);
    }
    mesh->indices = { 0, 1, 2 }; // minimal (not rendered in unit tests)

    // Compute AABB via CreateFromPoints (mirrors SceneLoader logic)
    std::vector<XMFLOAT3> positions;
    for (const auto& v : mesh->vertices)
        positions.push_back(v.position);
    BoundingBox::CreateFromPoints(mesh->aabb, positions.size(), positions.data(), sizeof(XMFLOAT3));
    return mesh;
}

} // anonymous namespace

// ── Mesh AABB ────────────────────────────────────────────────────────────────

TEST(MeshAABB, CreateFromPoints_CorrectCenterAndExtents)
{
    auto mesh = MakeMesh({ -1.0f, -2.0f, -3.0f }, { 1.0f, 2.0f, 3.0f });

    EXPECT_NEAR(mesh->aabb.Center.x, 0.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Center.y, 0.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Center.z, 0.0f, 1e-4f);

    EXPECT_NEAR(mesh->aabb.Extents.x, 1.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Extents.y, 2.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Extents.z, 3.0f, 1e-4f);
}

TEST(MeshAABB, CreateFromPoints_OffCenter)
{
    auto mesh = MakeMesh({ 2.0f, 4.0f, 6.0f }, { 4.0f, 8.0f, 10.0f });

    EXPECT_NEAR(mesh->aabb.Center.x, 3.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Center.y, 6.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Center.z, 8.0f, 1e-4f);

    EXPECT_NEAR(mesh->aabb.Extents.x, 1.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Extents.y, 2.0f, 1e-4f);
    EXPECT_NEAR(mesh->aabb.Extents.z, 2.0f, 1e-4f);
}

// ── SceneNode WorldAABB ───────────────────────────────────────────────────────

TEST(SceneNodeAABB, NoMesh_ReturnsDefaultAABB)
{
    RRE::SceneNode node;
    // no mesh assigned → should return default (zero-size) BoundingBox
    BoundingBox aabb = node.GetWorldAABB();
    EXPECT_NEAR(aabb.Center.x, 0.0f, 1e-4f);
    EXPECT_NEAR(aabb.Extents.x, 0.0f, 1e-4f);
}

TEST(SceneNodeAABB, IdentityTransform_MatchesMeshAABB)
{
    auto mesh = MakeMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    RRE::SceneNode node;
    node.SetMesh(mesh.get());

    BoundingBox worldAABB = node.GetWorldAABB();

    EXPECT_NEAR(worldAABB.Center.x,  0.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.y,  0.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.z,  0.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Extents.x, 1.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Extents.y, 1.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Extents.z, 1.0f, 1e-3f);
}

TEST(SceneNodeAABB, Translation_ShiftsAABBCenter)
{
    auto mesh = MakeMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    RRE::SceneNode node;
    node.SetMesh(mesh.get());
    node.GetTransform().SetPosition({ 5.0f, 0.0f, 0.0f });

    BoundingBox worldAABB = node.GetWorldAABB();

    EXPECT_NEAR(worldAABB.Center.x, 5.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.y, 0.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.z, 0.0f, 1e-3f);
    // Extents should be unchanged by pure translation
    EXPECT_NEAR(worldAABB.Extents.x, 1.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Extents.y, 1.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Extents.z, 1.0f, 1e-3f);
}

TEST(SceneNodeAABB, ParentTranslation_ChildWorldAABBCorrect)
{
    RRE::SceneGraph sceneGraph;
    auto mesh = MakeMesh({ -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f });

    // Parent at (10, 0, 0)
    auto parentNode = std::make_unique<RRE::SceneNode>();
    parentNode->GetTransform().SetPosition({ 10.0f, 0.0f, 0.0f });
    auto* parent = sceneGraph.GetRoot()->AddChild(std::move(parentNode));

    // Child at (0, 0, 0) relative to parent, with mesh
    auto childNode = std::make_unique<RRE::SceneNode>();
    childNode->SetMesh(mesh.get());
    auto* child = parent->AddChild(std::move(childNode));

    BoundingBox worldAABB = child->GetWorldAABB();

    // Child world center should be at parent's position (10, 0, 0)
    EXPECT_NEAR(worldAABB.Center.x, 10.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.y,  0.0f, 1e-3f);
    EXPECT_NEAR(worldAABB.Center.z,  0.0f, 1e-3f);
}

TEST(SceneNodeAABB, DirtyFlag_UpdatesAfterMeshChange)
{
    auto meshA = MakeMesh({ -1.0f, -1.0f, -1.0f }, {  1.0f,  1.0f,  1.0f });
    auto meshB = MakeMesh({  4.0f,  4.0f,  4.0f }, { 10.0f, 10.0f, 10.0f });

    RRE::SceneNode node;
    node.SetMesh(meshA.get());
    BoundingBox aabbA = node.GetWorldAABB();
    EXPECT_NEAR(aabbA.Center.x, 0.0f, 1e-3f);

    // Switch mesh — dirty flag must be set
    node.SetMesh(meshB.get());
    BoundingBox aabbB = node.GetWorldAABB();
    EXPECT_NEAR(aabbB.Center.x, 7.0f, 1e-3f);  // (4+10)/2 = 7
}
