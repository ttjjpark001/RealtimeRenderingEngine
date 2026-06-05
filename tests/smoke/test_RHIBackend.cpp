#include <gtest/gtest.h>
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Context.h"
#include "RHI/RHIContext.h"
#include <windows.h>

namespace
{

// Helper: create a minimal hidden window for swap chain
HWND CreateTestWindow()
{
    static bool registered = false;
    static const wchar_t* CLASS_NAME = L"RHITestWindowClass";

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

    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"RHITest",
        WS_OVERLAPPEDWINDOW, 0, 0, 320, 240,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    return hwnd;
}

} // anonymous namespace

TEST(RHIBackend, WARPDeviceInitAndShutdown)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    bool result = device.InitializeWARP(hwnd, 320, 240);
    EXPECT_TRUE(result);

    if (result)
    {
        device.Shutdown();
    }

    DestroyWindow(hwnd);
}

TEST(RHIBackend, ClearAndEndFrame)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    bool result = device.InitializeWARP(hwnd, 320, 240);
    ASSERT_TRUE(result);

    RRE::IRHIContext* context = device.GetContext();
    ASSERT_NE(context, nullptr);

    // Run one frame cycle
    context->BeginFrame();
    DirectX::XMFLOAT4 cobaltBlue(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(cobaltBlue);
    context->EndFrame();

    device.Shutdown();
    DestroyWindow(hwnd);
}

TEST(RHIBackend, OnResize)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    bool result = device.InitializeWARP(hwnd, 320, 240);
    ASSERT_TRUE(result);

    // Resize
    device.OnResize(640, 480);

    // Render after resize
    RRE::IRHIContext* context = device.GetContext();
    context->BeginFrame();
    DirectX::XMFLOAT4 color(0.0f, 0.28f, 0.67f, 1.0f);
    context->Clear(color);
    context->EndFrame();

    device.Shutdown();
    DestroyWindow(hwnd);
}

TEST(RHIBackend, CubeShadowMaps_CreateDoesNotCrash)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    ASSERT_TRUE(device.InitializeWARP(hwnd, 320, 240));

    auto* ctx = static_cast<RRE::D3D12Context*>(device.GetContext());
    ASSERT_NE(ctx, nullptr);

    EXPECT_NO_THROW(ctx->CreateCubeShadowMaps());
    EXPECT_NO_THROW(ctx->CreateCubeShadowMaps());  // idempotent

    device.Shutdown();
    DestroyWindow(hwnd);
}

TEST(RHIBackend, CubeShadowMaps_DefaultSizeIs512)
{
    HWND hwnd = CreateTestWindow();
    ASSERT_NE(hwnd, nullptr);

    RRE::D3D12Device device;
    ASSERT_TRUE(device.InitializeWARP(hwnd, 320, 240));

    auto* ctx = static_cast<RRE::D3D12Context*>(device.GetContext());
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->GetCubeShadowMapSize(), 512u);

    device.Shutdown();
    DestroyWindow(hwnd);
}
