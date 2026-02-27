#include <gtest/gtest.h>
#include "Asset/Material.h"

TEST(Material, DefaultValues)
{
    RRE::Material mat;

    // PBR factor defaults
    EXPECT_FLOAT_EQ(mat.baseColorFactor.x, 1.0f);
    EXPECT_FLOAT_EQ(mat.baseColorFactor.y, 1.0f);
    EXPECT_FLOAT_EQ(mat.baseColorFactor.z, 1.0f);
    EXPECT_FLOAT_EQ(mat.baseColorFactor.w, 1.0f);
    EXPECT_FLOAT_EQ(mat.metallicFactor, 1.0f);
    EXPECT_FLOAT_EQ(mat.roughnessFactor, 1.0f);
    EXPECT_FLOAT_EQ(mat.emissiveFactor.x, 0.0f);
    EXPECT_FLOAT_EQ(mat.emissiveFactor.y, 0.0f);
    EXPECT_FLOAT_EQ(mat.emissiveFactor.z, 0.0f);

    // Render state defaults
    EXPECT_EQ(mat.alphaMode, RRE::AlphaMode::Opaque);
    EXPECT_FLOAT_EQ(mat.alphaCutoff, 0.5f);
    EXPECT_FALSE(mat.doubleSided);

    // Texture pointers default to null
    EXPECT_EQ(mat.baseColorTexture, nullptr);
    EXPECT_EQ(mat.metallicRoughnessTexture, nullptr);
    EXPECT_EQ(mat.normalTexture, nullptr);
    EXPECT_EQ(mat.emissiveTexture, nullptr);
    EXPECT_EQ(mat.occlusionTexture, nullptr);

    // Texture paths default to empty
    EXPECT_TRUE(mat.baseColorTexturePath.empty());
    EXPECT_TRUE(mat.metallicRoughnessTexturePath.empty());
    EXPECT_TRUE(mat.normalTexturePath.empty());
    EXPECT_TRUE(mat.emissiveTexturePath.empty());
    EXPECT_TRUE(mat.occlusionTexturePath.empty());
}

TEST(Material, DirtyFlagInitiallyTrue)
{
    RRE::Material mat;
    EXPECT_TRUE(mat.IsDirty());
}

TEST(Material, ClearDirtySetsFalse)
{
    RRE::Material mat;
    mat.ClearDirty();
    EXPECT_FALSE(mat.IsDirty());
}

TEST(Material, SettersMakeDirty)
{
    RRE::Material mat;
    mat.ClearDirty();
    EXPECT_FALSE(mat.IsDirty());

    mat.SetBaseColorFactor({ 0.5f, 0.5f, 0.5f, 1.0f });
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetMetallicFactor(0.3f);
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetRoughnessFactor(0.7f);
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetEmissiveFactor({ 1.0f, 0.0f, 0.0f });
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetAlphaMode(RRE::AlphaMode::Blend);
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetAlphaCutoff(0.8f);
    EXPECT_TRUE(mat.IsDirty());

    mat.ClearDirty();
    mat.SetDoubleSided(true);
    EXPECT_TRUE(mat.IsDirty());
}

TEST(Material, SetterValuesApplied)
{
    RRE::Material mat;
    mat.SetBaseColorFactor({ 0.2f, 0.3f, 0.4f, 0.5f });
    EXPECT_FLOAT_EQ(mat.baseColorFactor.x, 0.2f);
    EXPECT_FLOAT_EQ(mat.baseColorFactor.w, 0.5f);

    mat.SetMetallicFactor(0.1f);
    EXPECT_FLOAT_EQ(mat.metallicFactor, 0.1f);

    mat.SetAlphaMode(RRE::AlphaMode::Mask);
    EXPECT_EQ(mat.alphaMode, RRE::AlphaMode::Mask);

    mat.SetDoubleSided(true);
    EXPECT_TRUE(mat.doubleSided);
}
