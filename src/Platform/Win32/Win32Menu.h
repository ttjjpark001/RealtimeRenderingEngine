#pragma once

#include <Windows.h>
#include <functional>
#include "Core/Types.h"

namespace RRE
{

enum class MeshType
{
    Sphere,
    Tetrahedron,
    Cube,
    Cylinder
};

// Menu command IDs
constexpr UINT ID_VIEW_800x450      = 1001;
constexpr UINT ID_VIEW_960x540      = 1002;
constexpr UINT ID_VIEW_FULLSCREEN   = 1003;

constexpr UINT ID_OBJECT_SPHERE     = 2001;
constexpr UINT ID_OBJECT_TETRAHEDRON = 2002;
constexpr UINT ID_OBJECT_CUBE       = 2003;
constexpr UINT ID_OBJECT_CYLINDER   = 2004;

constexpr UINT ID_ANIM_PLAY         = 3001;
constexpr UINT ID_ANIM_PAUSE        = 3002;

constexpr UINT ID_LIGHT_SHOW_INFO   = 4001;
constexpr UINT ID_LIGHT_WHITE       = 4002;
constexpr UINT ID_LIGHT_RED         = 4003;
constexpr UINT ID_LIGHT_GREEN       = 4004;
constexpr UINT ID_LIGHT_BLUE        = 4005;
constexpr UINT ID_LIGHT_YELLOW      = 4006;
constexpr UINT ID_LIGHT_CYAN        = 4007;
constexpr UINT ID_LIGHT_MAGENTA     = 4008;
constexpr UINT ID_LIGHT_RESET_POS   = 4009;

class Win32Menu
{
public:
    using ViewCallback = std::function<void(uint32, uint32, bool)>;
    using MeshCallback = std::function<void(MeshType)>;
    using AnimCallback = std::function<void()>;
    using LightColorCallback = std::function<void(float, float, float)>;
    using LightToggleInfoCallback = std::function<void()>;
    using LightResetCallback = std::function<void()>;

    Win32Menu() = default;
    ~Win32Menu() = default;

    bool Initialize(HWND hwnd);

    bool HandleCommand(WPARAM wParam);

    void SetViewCallback(ViewCallback callback) { m_viewCallback = std::move(callback); }
    void SetMeshCallback(MeshCallback callback) { m_meshCallback = std::move(callback); }
    void SetAnimCallback(AnimCallback callback) { m_animCallback = std::move(callback); }
    void SetLightColorCallback(LightColorCallback callback) { m_lightColorCallback = std::move(callback); }
    void SetLightToggleInfoCallback(LightToggleInfoCallback callback) { m_lightToggleInfoCallback = std::move(callback); }
    void SetLightResetCallback(LightResetCallback callback) { m_lightResetCallback = std::move(callback); }

    void UpdateAnimCheckMark(bool isPlaying);

private:
    HWND m_hwnd = nullptr;
    HMENU m_menuBar = nullptr;
    HMENU m_viewMenu = nullptr;
    HMENU m_objectMenu = nullptr;
    HMENU m_animMenu = nullptr;
    HMENU m_lightMenu = nullptr;

    ViewCallback m_viewCallback;
    MeshCallback m_meshCallback;
    AnimCallback m_animCallback;
    LightColorCallback m_lightColorCallback;
    LightToggleInfoCallback m_lightToggleInfoCallback;
    LightResetCallback m_lightResetCallback;
};

} // namespace RRE
