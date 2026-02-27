#include <gtest/gtest.h>
#include "Asset/SceneLoader.h"
#include <filesystem>

namespace fs = std::filesystem;

// Find the project root by searching up from the executable directory
static std::string FindProjectRoot()
{
    // Try common paths relative to test executable location
    fs::path candidates[] = {
        fs::current_path(),
        fs::current_path() / ".." / "..",       // bin/Debug/ -> root
        fs::current_path() / ".." / ".." / "..", // deeper nesting
    };

    for (const auto& candidate : candidates)
    {
        fs::path testFile = candidate / "assets" / "test" / "triangle.gltf";
        if (fs::exists(testFile))
            return fs::canonical(candidate).string();
    }

    // Fallback: try absolute path
    fs::path absolute("E:/Vibe Coding/Claude Code/RealtimeRenderingEngine");
    if (fs::exists(absolute / "assets" / "test" / "triangle.gltf"))
        return absolute.string();

    return "";
}

class SceneLoaderTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_projectRoot = FindProjectRoot();
        ASSERT_FALSE(m_projectRoot.empty()) << "Could not find project root with test assets";
    }

    std::string GetTestAssetPath(const std::string& relativePath) const
    {
        return (fs::path(m_projectRoot) / "assets" / "test" / relativePath).string();
    }

    std::string m_projectRoot;
};

TEST_F(SceneLoaderTest, LoadTriangleGltf_HasMeshes)
{
    RRE::SceneLoader loader;
    RRE::SceneData data = loader.LoadScene(GetTestAssetPath("triangle.gltf"));

    EXPECT_GT(data.meshes.size(), 0u) << "Should have at least one mesh";
}

TEST_F(SceneLoaderTest, LoadTriangleGltf_HasRootNode)
{
    RRE::SceneLoader loader;
    RRE::SceneData data = loader.LoadScene(GetTestAssetPath("triangle.gltf"));

    EXPECT_NE(data.rootNode, nullptr) << "Root node should not be null";
}

TEST_F(SceneLoaderTest, LoadTriangleGltf_MeshHasVerticesAndIndices)
{
    RRE::SceneLoader loader;
    RRE::SceneData data = loader.LoadScene(GetTestAssetPath("triangle.gltf"));

    ASSERT_GT(data.meshes.size(), 0u);
    const auto& mesh = data.meshes[0];
    EXPECT_EQ(mesh->vertices.size(), 3u) << "Triangle should have 3 vertices";
    EXPECT_EQ(mesh->indices.size(), 3u) << "Triangle should have 3 indices";
}

TEST_F(SceneLoaderTest, LoadTriangleGltf_BoundingBoxIsValid)
{
    RRE::SceneLoader loader;
    RRE::SceneData data = loader.LoadScene(GetTestAssetPath("triangle.gltf"));

    EXPECT_TRUE(data.sceneBounds.IsValid()) << "Scene bounding box should be valid";
    // Triangle spans from (-1,0,0) to (1,1,0)
    EXPECT_LE(data.sceneBounds.min.x, -0.99f);
    EXPECT_GE(data.sceneBounds.max.x, 0.99f);
    EXPECT_GE(data.sceneBounds.max.y, 0.99f);
}

TEST_F(SceneLoaderTest, LoadInvalidFile_Throws)
{
    RRE::SceneLoader loader;
    EXPECT_THROW(loader.LoadScene("nonexistent_file.gltf"), std::runtime_error);
}
