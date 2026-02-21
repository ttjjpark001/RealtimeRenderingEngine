#include <gtest/gtest.h>
#include "Renderer/MeshFactory.h"
#include "Renderer/FaceColorPalette.h"

using namespace RRE;

namespace
{

// Verify no adjacent faces share the same color
void VerifyAdjacentFacesHaveDifferentColors(const Mesh& mesh)
{
    uint32 faceCount = mesh.GetPolygonCount();
    ASSERT_EQ(mesh.faceAdjacency.size(), faceCount);

    // Extract face color palette index from vertex color
    // Each face has 3 vertices with the same color
    for (uint32 f = 0; f < faceCount; ++f)
    {
        // Get this face's color from its first vertex
        uint32 vertIdx = f * 3;
        const auto& faceColor = mesh.vertices[vertIdx].color;

        // Verify all 3 vertices of this face have the same color
        for (int v = 1; v < 3; ++v)
        {
            const auto& vc = mesh.vertices[vertIdx + v].color;
            EXPECT_FLOAT_EQ(faceColor.x, vc.x);
            EXPECT_FLOAT_EQ(faceColor.y, vc.y);
            EXPECT_FLOAT_EQ(faceColor.z, vc.z);
            EXPECT_FLOAT_EQ(faceColor.w, vc.w);
        }

        // Check against all adjacent faces
        for (uint32 neighbor : mesh.faceAdjacency[f])
        {
            uint32 neighborVertIdx = neighbor * 3;
            const auto& neighborColor = mesh.vertices[neighborVertIdx].color;

            bool sameColor =
                faceColor.x == neighborColor.x &&
                faceColor.y == neighborColor.y &&
                faceColor.z == neighborColor.z;

            EXPECT_FALSE(sameColor) << "Face " << f << " and face " << neighbor
                << " share the same color but are adjacent";
        }
    }
}

// Verify all face colors are valid palette colors
void VerifyValidPaletteColors(const Mesh& mesh)
{
    uint32 faceCount = mesh.GetPolygonCount();
    for (uint32 f = 0; f < faceCount; ++f)
    {
        uint32 vertIdx = f * 3;
        const auto& faceColor = mesh.vertices[vertIdx].color;

        bool found = false;
        for (uint32 c = 0; c < FaceColorPalette::PALETTE_SIZE; ++c)
        {
            const auto& paletteColor = FaceColorPalette::GetColor(c);
            if (faceColor.x == paletteColor.x &&
                faceColor.y == paletteColor.y &&
                faceColor.z == paletteColor.z &&
                faceColor.w == paletteColor.w)
            {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Face " << f << " has a color not in the palette";
    }
}

} // anonymous namespace

TEST(FaceColoring, TetrahedronAdjacentFacesDifferent)
{
    Mesh mesh = MeshFactory::CreateTetrahedron();
    EXPECT_EQ(mesh.GetPolygonCount(), 4u);
    VerifyAdjacentFacesHaveDifferentColors(mesh);
    VerifyValidPaletteColors(mesh);
}

TEST(FaceColoring, CubeAdjacentFacesDifferent)
{
    Mesh mesh = MeshFactory::CreateCube();
    EXPECT_EQ(mesh.GetPolygonCount(), 12u); // 6 faces * 2 triangles
    VerifyAdjacentFacesHaveDifferentColors(mesh);
    VerifyValidPaletteColors(mesh);
}

TEST(FaceColoring, SphereAdjacentFacesDifferent)
{
    Mesh mesh = MeshFactory::CreateSphere(8, 8);
    EXPECT_GT(mesh.GetPolygonCount(), 0u);
    VerifyAdjacentFacesHaveDifferentColors(mesh);
    VerifyValidPaletteColors(mesh);
}

TEST(FaceColoring, CylinderAdjacentFacesDifferent)
{
    Mesh mesh = MeshFactory::CreateCylinder(8, 2.0f);
    EXPECT_GT(mesh.GetPolygonCount(), 0u);
    VerifyAdjacentFacesHaveDifferentColors(mesh);
    VerifyValidPaletteColors(mesh);
}
