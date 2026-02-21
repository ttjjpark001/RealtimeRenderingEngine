#include <gtest/gtest.h>
#include "Scene/Camera.h"

using namespace DirectX;
using namespace RRE;

TEST(Camera, DefaultPerspectiveProjection)
{
    Camera cam;
    XMMATRIX proj = cam.GetProjectionMatrix(16.0f / 9.0f);
    // Should produce a valid non-zero matrix
    XMFLOAT4X4 mat;
    XMStoreFloat4x4(&mat, proj);
    EXPECT_NE(mat._11, 0.0f);
    EXPECT_NE(mat._22, 0.0f);
}

TEST(Camera, OrthographicProjection)
{
    Camera cam;
    cam.SetProjectionMode(ProjectionMode::Orthographic);
    XMMATRIX proj = cam.GetProjectionMatrix(16.0f / 9.0f);
    XMFLOAT4X4 mat;
    XMStoreFloat4x4(&mat, proj);
    EXPECT_NE(mat._11, 0.0f);
    EXPECT_NE(mat._22, 0.0f);
}

TEST(Camera, ProjectionModeSwitch)
{
    Camera cam;
    EXPECT_EQ(cam.GetProjectionMode(), ProjectionMode::Perspective);
    cam.SetProjectionMode(ProjectionMode::Orthographic);
    EXPECT_EQ(cam.GetProjectionMode(), ProjectionMode::Orthographic);
    EXPECT_STREQ(cam.GetProjectionModeName(), "Orthographic");
    cam.SetProjectionMode(ProjectionMode::Perspective);
    EXPECT_STREQ(cam.GetProjectionModeName(), "Perspective");
}

TEST(Camera, MoveForwardChangesPosition)
{
    Camera cam;
    XMFLOAT3 before = cam.GetPosition();
    cam.MoveForward(1.0f);
    XMFLOAT3 after = cam.GetPosition();
    // Camera default looks from (0,0,-5) toward (0,0,0), so forward is +Z
    EXPECT_GT(after.z, before.z);
}

TEST(Camera, MoveRightChangesPosition)
{
    Camera cam;
    XMFLOAT3 before = cam.GetPosition();
    cam.MoveRight(1.0f);
    XMFLOAT3 after = cam.GetPosition();
    EXPECT_NE(after.x, before.x);
}

TEST(Camera, MoveUpChangesPosition)
{
    Camera cam;
    XMFLOAT3 before = cam.GetPosition();
    cam.MoveUp(1.0f);
    XMFLOAT3 after = cam.GetPosition();
    EXPECT_GT(after.y, before.y);
}

TEST(Camera, FovClampMin)
{
    Camera cam;
    cam.AdjustFov(-1000.0f);
    float deg = cam.GetFovDegrees();
    EXPECT_NEAR(deg, 10.0f, 0.1f);
}

TEST(Camera, FovClampMax)
{
    Camera cam;
    cam.AdjustFov(1000.0f);
    float deg = cam.GetFovDegrees();
    EXPECT_NEAR(deg, 120.0f, 0.1f);
}

TEST(Camera, GetDirectionIsNormalized)
{
    Camera cam;
    XMFLOAT3 dir = cam.GetDirection();
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    EXPECT_NEAR(len, 1.0f, 0.001f);
}

TEST(Camera, ViewMatrixIsValid)
{
    Camera cam;
    XMMATRIX view = cam.GetViewMatrix();
    XMFLOAT4X4 mat;
    XMStoreFloat4x4(&mat, view);
    // Check that the matrix is not all zeros
    bool nonZero = false;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (mat.m[i][j] != 0.0f) nonZero = true;
    EXPECT_TRUE(nonZero);
}

TEST(Camera, ResetRestoresDefaults)
{
    Camera cam;
    cam.MoveForward(10.0f);
    cam.AdjustFov(30.0f);
    cam.SetProjectionMode(ProjectionMode::Orthographic);
    cam.Reset();
    EXPECT_NEAR(cam.GetPosition().z, -5.0f, 0.001f);
    EXPECT_NEAR(cam.GetFovDegrees(), 45.0f, 0.1f);
    EXPECT_EQ(cam.GetProjectionMode(), ProjectionMode::Perspective);
}
