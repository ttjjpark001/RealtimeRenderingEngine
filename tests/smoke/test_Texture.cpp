#include <gtest/gtest.h>
#include "Asset/Texture.h"
#include "Asset/TextureCache.h"
#include "RHI/D3D12/D3D12Device.h"
#include "RHI/D3D12/D3D12Context.h"
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace
{

HWND CreateTestWindow()
{
    static bool registered = false;
    static const wchar_t* CLASS_NAME = L"TextureTestWindowClass";

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

    return CreateWindowExW(0, CLASS_NAME, L"TextureTest",
        WS_OVERLAPPEDWINDOW, 0, 0, 320, 240,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
}

static std::string FindProjectRoot()
{
    fs::path candidates[] = {
        fs::current_path(),
        fs::current_path() / ".." / "..",
        fs::current_path() / ".." / ".." / "..",
    };

    for (const auto& candidate : candidates)
    {
        fs::path testFile = candidate / "assets" / "test" / "test_texture.png";
        if (fs::exists(testFile))
            return fs::canonical(candidate).string();
    }

    fs::path absolute("E:/Vibe Coding/Claude Code/RealtimeRenderingEngine");
    if (fs::exists(absolute / "assets" / "test" / "test_texture.png"))
        return absolute.string();

    return "";
}

} // anonymous namespace

class TextureTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_hwnd = CreateTestWindow();
        ASSERT_NE(m_hwnd, nullptr);

        m_device = std::make_unique<RRE::D3D12Device>();
        bool result = m_device->InitializeWARP(m_hwnd, 320, 240);
        ASSERT_TRUE(result) << "Failed to initialize WARP device";

        m_d3dDevice = m_device->GetD3DDevice();
        ASSERT_NE(m_d3dDevice, nullptr);

        auto* context = static_cast<RRE::D3D12Context*>(m_device->GetContext());
        m_cmdList = context->GetCommandList();
        ASSERT_NE(m_cmdList, nullptr);

        // Begin a frame so the command list is in recording state
        context->BeginFrame();

        m_projectRoot = FindProjectRoot();
    }

    void TearDown() override
    {
        auto* context = static_cast<RRE::D3D12Context*>(m_device->GetContext());
        // End the frame and wait for GPU
        DirectX::XMFLOAT4 black(0, 0, 0, 1);
        context->Clear(black);
        context->EndFrame();
        context->WaitForGPU();

        m_device->Shutdown();
        if (m_hwnd) DestroyWindow(m_hwnd);
    }

    std::string GetTestAssetPath(const std::string& relativePath) const
    {
        return (fs::path(m_projectRoot) / "assets" / "test" / relativePath).string();
    }

    HWND m_hwnd = nullptr;
    std::unique_ptr<RRE::D3D12Device> m_device;
    ID3D12Device* m_d3dDevice = nullptr;
    ID3D12GraphicsCommandList* m_cmdList = nullptr;
    std::string m_projectRoot;
};

TEST_F(TextureTest, CreateFromData_CreatesResource)
{
    // 2x2 white RGBA pixels
    const RRE::uint8 pixels[] = {
        255, 255, 255, 255,  255, 255, 255, 255,
        255, 255, 255, 255,  255, 255, 255, 255
    };

    RRE::Texture texture;
    bool result = texture.CreateFromData(m_d3dDevice, m_cmdList, pixels, 2, 2,
                                          DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_TRUE(result);
    EXPECT_NE(texture.GetResource(), nullptr);
    EXPECT_EQ(texture.GetWidth(), 2u);
    EXPECT_EQ(texture.GetHeight(), 2u);
    EXPECT_EQ(texture.GetState(), RRE::TextureState::Ready);
}

TEST_F(TextureTest, CreateFallback_Creates1x1White)
{
    auto fallback = RRE::Texture::CreateFallback(m_d3dDevice, m_cmdList);
    ASSERT_NE(fallback, nullptr);
    EXPECT_NE(fallback->GetResource(), nullptr);
    EXPECT_EQ(fallback->GetWidth(), 1u);
    EXPECT_EQ(fallback->GetHeight(), 1u);
    EXPECT_EQ(fallback->GetState(), RRE::TextureState::Ready);
}

TEST_F(TextureTest, TextureCache_InitializeCreatesFallback)
{
    RRE::TextureCache cache;
    bool result = cache.Initialize(m_d3dDevice, m_cmdList);
    EXPECT_TRUE(result);
    EXPECT_NE(cache.GetFallback(), nullptr);
    EXPECT_NE(cache.GetFallback()->GetResource(), nullptr);
}

TEST_F(TextureTest, TextureCache_GetOrLoad_EmptyPathReturnsFallback)
{
    RRE::TextureCache cache;
    ASSERT_TRUE(cache.Initialize(m_d3dDevice, m_cmdList));

    RRE::Texture* tex = cache.GetOrLoad("", false, m_d3dDevice, m_cmdList);
    EXPECT_EQ(tex, cache.GetFallback());
}

TEST_F(TextureTest, TextureCache_GetOrLoad_InvalidPathReturnsFallback)
{
    RRE::TextureCache cache;
    ASSERT_TRUE(cache.Initialize(m_d3dDevice, m_cmdList));

    RRE::Texture* tex = cache.GetOrLoad("nonexistent_file.png", false, m_d3dDevice, m_cmdList);
    EXPECT_EQ(tex, cache.GetFallback());
}

TEST_F(TextureTest, TextureCache_GetOrLoad_ValidImage)
{
    if (m_projectRoot.empty())
    {
        GTEST_SKIP() << "Test assets not found";
    }

    RRE::TextureCache cache;
    ASSERT_TRUE(cache.Initialize(m_d3dDevice, m_cmdList));

    std::string path = GetTestAssetPath("test_texture.png");
    RRE::Texture* tex = cache.GetOrLoad(path, true, m_d3dDevice, m_cmdList);

    ASSERT_NE(tex, nullptr);
    EXPECT_NE(tex, cache.GetFallback()) << "Should load actual texture, not fallback";
    EXPECT_NE(tex->GetResource(), nullptr);
    EXPECT_EQ(tex->GetWidth(), 2u);
    EXPECT_EQ(tex->GetHeight(), 2u);
    EXPECT_EQ(tex->GetFormat(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    EXPECT_EQ(tex->GetState(), RRE::TextureState::Ready);
}

TEST_F(TextureTest, TextureCache_GetOrLoad_CachesTexture)
{
    if (m_projectRoot.empty())
    {
        GTEST_SKIP() << "Test assets not found";
    }

    RRE::TextureCache cache;
    ASSERT_TRUE(cache.Initialize(m_d3dDevice, m_cmdList));

    std::string path = GetTestAssetPath("test_texture.png");
    RRE::Texture* tex1 = cache.GetOrLoad(path, true, m_d3dDevice, m_cmdList);
    RRE::Texture* tex2 = cache.GetOrLoad(path, true, m_d3dDevice, m_cmdList);

    EXPECT_EQ(tex1, tex2) << "Same path should return same cached texture";
    EXPECT_EQ(cache.GetCachedCount(), 1u) << "Should only have one cached texture";
}

TEST_F(TextureTest, TextureCache_Clear_RemovesAll)
{
    if (m_projectRoot.empty())
    {
        GTEST_SKIP() << "Test assets not found";
    }

    RRE::TextureCache cache;
    ASSERT_TRUE(cache.Initialize(m_d3dDevice, m_cmdList));

    std::string path = GetTestAssetPath("test_texture.png");
    cache.GetOrLoad(path, true, m_d3dDevice, m_cmdList);
    ASSERT_EQ(cache.GetCachedCount(), 1u);

    cache.Clear();
    EXPECT_EQ(cache.GetCachedCount(), 0u);
    EXPECT_NE(cache.GetFallback(), nullptr) << "Fallback should survive Clear()";
}
