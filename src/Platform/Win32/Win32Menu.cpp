#include "Platform/Win32/Win32Menu.h"

namespace RRE
{

bool Win32Menu::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    m_menuBar = CreateMenu();
    if (!m_menuBar)
        return false;

    // File menu (first in menu bar)
    m_fileMenu = CreatePopupMenu();
    AppendMenuW(m_fileMenu, MF_STRING, ID_FILE_OPEN_SCENE,  L"Open Scene...\tCtrl+O");
    AppendMenuW(m_fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_fileMenu, MF_STRING, ID_FILE_OPEN_SPONZA, L"Sponza!");
    AppendMenuW(m_fileMenu, MF_STRING, ID_FILE_OPEN_BISTRO, L"Bistro!");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_fileMenu), L"File");

    // View menu
    m_viewMenu = CreatePopupMenu();
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_1440x810, L"1440 x 810");
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_1600x900, L"1600 x 900");
    AppendMenuW(m_viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_FULLSCREEN, L"Full Screen");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_viewMenu), L"View");

    // Default check: 1600x900
    CheckMenuRadioItem(m_viewMenu, ID_VIEW_1440x810, ID_VIEW_1600x900,
        ID_VIEW_1600x900, MF_BYCOMMAND);

    // Light menu
    m_lightMenu = CreatePopupMenu();
    AppendMenuW(m_lightMenu, MF_STRING | MF_CHECKED, ID_LIGHT_SHOW_INFO, L"Show Info");
    AppendMenuW(m_lightMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_WHITE, L"White");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_RED, L"Red");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_GREEN, L"Green");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_BLUE, L"Blue");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_YELLOW, L"Yellow");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_CYAN, L"Cyan");
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_MAGENTA, L"Magenta");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_lightMenu), L"Light");

    // Default check: White
    CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA,
        ID_LIGHT_WHITE, MF_BYCOMMAND);

    // Camera menu
    m_cameraMenu = CreatePopupMenu();
    AppendMenuW(m_cameraMenu, MF_STRING | MF_CHECKED, ID_CAMERA_SHOW_INFO, L"Show Info");
    AppendMenuW(m_cameraMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_PERSPECTIVE, L"Perspective");
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_ORTHOGRAPHIC, L"Orthographic");
    AppendMenuW(m_cameraMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_FOV_UP, L"FOV+");
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_FOV_DOWN, L"FOV-");
    AppendMenuW(m_cameraMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_FIT_TO_SCENE, L"Fit to Scene");
    AppendMenuW(m_cameraMenu, MF_STRING, ID_CAMERA_RESET, L"Reset");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_cameraMenu), L"Camera");

    // Default check: Perspective
    CheckMenuRadioItem(m_cameraMenu, ID_CAMERA_PERSPECTIVE, ID_CAMERA_ORTHOGRAPHIC,
        ID_CAMERA_PERSPECTIVE, MF_BYCOMMAND);

    // Render menu
    m_renderMenu = CreatePopupMenu();
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_WIREFRAME, L"Wireframe");
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_SOLID, L"Solid (No Texture)");
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_BASECOLOR, L"Base Color Only");
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_FULL_PBR, L"Full PBR");
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_FULL_PBR_SHADOW, L"Full PBR + Shadows");
    AppendMenuW(m_renderMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_renderMenu, MF_STRING, ID_RENDER_CSM_DEBUG, L"CSM Cascade Debug View");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_renderMenu), L"Render");

    // Default check: Full PBR + Shadows
    CheckMenuRadioItem(m_renderMenu, ID_RENDER_WIREFRAME, ID_RENDER_FULL_PBR_SHADOW,
        ID_RENDER_FULL_PBR_SHADOW, MF_BYCOMMAND);

    // Optimization menu
    m_optimMenu = CreatePopupMenu();
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_FRUSTUM_CULL,   L"Frustum Culling");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_LIGHT_CULL,     L"Light Culling");
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_LOD,            L"LOD");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_MIPMAP,         L"MipMap");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED,  ID_OPTIM_OCCLUSION_CULL, L"Occlusion Culling (Hi-Z)");
    AppendMenuW(m_optimMenu, MF_SEPARATOR,           0,                       nullptr);
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_CSM,            L"CSM (Cascaded Shadow Maps)");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED,  ID_OPTIM_PCSS,           L"PCSS (Soft Shadows)");
    AppendMenuW(m_optimMenu, MF_SEPARATOR,           0,                       nullptr);
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_PCSS_SIZE_SMALL,  L"PCSS Light Size: Small (0.5x)");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_PCSS_SIZE_NORMAL, L"PCSS Light Size: Normal (1.0x)");
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_PCSS_SIZE_LARGE,  L"PCSS Light Size: Large (2.0x)");
    AppendMenuW(m_optimMenu, MF_SEPARATOR,           0,                         nullptr);
    AppendMenuW(m_optimMenu, MF_STRING | MF_GRAYED,  ID_OPTIM_TORCH_SHADOW_0,   L"Torch Shadows: Off");
    AppendMenuW(m_optimMenu, MF_STRING | MF_GRAYED,  ID_OPTIM_TORCH_SHADOW_1,   L"Torch Shadows: 1");
    AppendMenuW(m_optimMenu, MF_STRING | MF_GRAYED,  ID_OPTIM_TORCH_SHADOW_2,   L"Torch Shadows: 2");
    AppendMenuW(m_optimMenu, MF_STRING | MF_GRAYED,  ID_OPTIM_TORCH_SHADOW_3,   L"Torch Shadows: 3");
    AppendMenuW(m_optimMenu, MF_STRING | MF_GRAYED,  ID_OPTIM_TORCH_SHADOW_4,   L"Torch Shadows: All 4");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_optimMenu), L"Optimization");

    // Animation menu (Phase 34 Part A) — clips populated later via SetAnimationClips()
    m_animMenu = CreatePopupMenu();
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED, ID_ANIM_PLAY_PAUSE, L"Play / Pause");
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED, ID_ANIM_STOP,       L"Stop");
    AppendMenuW(m_animMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED, ID_ANIM_SPEED_HALF,   L"Speed: 0.5x");
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED, ID_ANIM_SPEED_NORMAL, L"Speed: 1.0x");
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED, ID_ANIM_SPEED_DOUBLE, L"Speed: 2.0x");
    AppendMenuW(m_animMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, L"(No Animation)");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_animMenu), L"Animation");

    SetMenu(hwnd, m_menuBar);
    return true;
}

bool Win32Menu::HandleCommand(WPARAM wParam)
{
    UINT id = LOWORD(wParam);

    switch (id)
    {
    // File commands
    case ID_FILE_OPEN_SCENE:
        if (m_fileOpenCallback) m_fileOpenCallback();
        return true;

    case ID_FILE_OPEN_SPONZA:
        if (m_fileSponzaCallback) m_fileSponzaCallback();
        return true;

    case ID_FILE_OPEN_BISTRO:
        if (m_fileBistroCallback) m_fileBistroCallback();
        return true;

    // View commands
    case ID_VIEW_1440x810:
        CheckMenuRadioItem(m_viewMenu, ID_VIEW_1440x810, ID_VIEW_1600x900,
            ID_VIEW_1440x810, MF_BYCOMMAND);
        if (m_viewCallback) m_viewCallback(1440, 810, false);
        return true;

    case ID_VIEW_1600x900:
        CheckMenuRadioItem(m_viewMenu, ID_VIEW_1440x810, ID_VIEW_1600x900,
            ID_VIEW_1600x900, MF_BYCOMMAND);
        if (m_viewCallback) m_viewCallback(1600, 900, false);
        return true;

    case ID_VIEW_FULLSCREEN:
        if (m_viewCallback) m_viewCallback(0, 0, true);
        return true;

    // Light commands
    case ID_LIGHT_SHOW_INFO:
    {
        UINT state = GetMenuState(m_lightMenu, ID_LIGHT_SHOW_INFO, MF_BYCOMMAND);
        CheckMenuItem(m_lightMenu, ID_LIGHT_SHOW_INFO,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_lightToggleInfoCallback) m_lightToggleInfoCallback();
        return true;
    }

    case ID_LIGHT_WHITE:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_WHITE, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(1.0f, 1.0f, 1.0f);
        return true;
    case ID_LIGHT_RED:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_RED, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(1.0f, 0.0f, 0.0f);
        return true;
    case ID_LIGHT_GREEN:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_GREEN, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(0.0f, 1.0f, 0.0f);
        return true;
    case ID_LIGHT_BLUE:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_BLUE, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(0.0f, 0.0f, 1.0f);
        return true;
    case ID_LIGHT_YELLOW:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_YELLOW, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(1.0f, 1.0f, 0.0f);
        return true;
    case ID_LIGHT_CYAN:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_CYAN, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(0.0f, 1.0f, 1.0f);
        return true;
    case ID_LIGHT_MAGENTA:
        CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA, ID_LIGHT_MAGENTA, MF_BYCOMMAND);
        if (m_lightColorCallback) m_lightColorCallback(1.0f, 0.0f, 1.0f);
        return true;

    // Camera commands
    case ID_CAMERA_SHOW_INFO:
    {
        UINT state = GetMenuState(m_cameraMenu, ID_CAMERA_SHOW_INFO, MF_BYCOMMAND);
        CheckMenuItem(m_cameraMenu, ID_CAMERA_SHOW_INFO,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_cameraToggleInfoCallback) m_cameraToggleInfoCallback();
        return true;
    }

    case ID_CAMERA_PERSPECTIVE:
        CheckMenuRadioItem(m_cameraMenu, ID_CAMERA_PERSPECTIVE, ID_CAMERA_ORTHOGRAPHIC,
            ID_CAMERA_PERSPECTIVE, MF_BYCOMMAND);
        if (m_cameraProjectionCallback) m_cameraProjectionCallback(true);
        return true;

    case ID_CAMERA_ORTHOGRAPHIC:
        CheckMenuRadioItem(m_cameraMenu, ID_CAMERA_PERSPECTIVE, ID_CAMERA_ORTHOGRAPHIC,
            ID_CAMERA_ORTHOGRAPHIC, MF_BYCOMMAND);
        if (m_cameraProjectionCallback) m_cameraProjectionCallback(false);
        return true;

    case ID_CAMERA_FOV_UP:
        if (m_cameraFovCallback) m_cameraFovCallback(5.0f);
        return true;

    case ID_CAMERA_FOV_DOWN:
        if (m_cameraFovCallback) m_cameraFovCallback(-5.0f);
        return true;

    case ID_CAMERA_FIT_TO_SCENE:
        if (m_cameraFitToSceneCallback) m_cameraFitToSceneCallback();
        return true;

    case ID_CAMERA_RESET:
        CheckMenuRadioItem(m_cameraMenu, ID_CAMERA_PERSPECTIVE, ID_CAMERA_ORTHOGRAPHIC,
            ID_CAMERA_PERSPECTIVE, MF_BYCOMMAND);
        if (m_cameraResetCallback) m_cameraResetCallback();
        return true;

    // Render mode commands
    case ID_RENDER_WIREFRAME:
    case ID_RENDER_SOLID:
    case ID_RENDER_BASECOLOR:
    case ID_RENDER_FULL_PBR:
    case ID_RENDER_FULL_PBR_SHADOW:
        CheckMenuRadioItem(m_renderMenu, ID_RENDER_WIREFRAME, ID_RENDER_FULL_PBR_SHADOW,
            id, MF_BYCOMMAND);
        if (m_renderModeCallback) m_renderModeCallback(id - ID_RENDER_WIREFRAME);
        return true;

    // Optimization commands
    case ID_OPTIM_FRUSTUM_CULL:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_FRUSTUM_CULL, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_FRUSTUM_CULL,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_frustumCullToggleCallback) m_frustumCullToggleCallback();
        return true;
    }

    case ID_OPTIM_LIGHT_CULL:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_LIGHT_CULL, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_LIGHT_CULL,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_lightCullToggleCallback) m_lightCullToggleCallback();
        return true;
    }

    case ID_OPTIM_LOD:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_LOD, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_LOD,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_lodToggleCallback) m_lodToggleCallback();
        return true;
    }

    case ID_OPTIM_MIPMAP:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_MIPMAP, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_MIPMAP,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_mipMapToggleCallback) m_mipMapToggleCallback();
        return true;
    }

    case ID_OPTIM_OCCLUSION_CULL:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_OCCLUSION_CULL, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_OCCLUSION_CULL,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_occlusionCullToggleCallback) m_occlusionCullToggleCallback();
        return true;
    }

    case ID_OPTIM_CSM:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_CSM, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_CSM,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_csmToggleCallback) m_csmToggleCallback();
        return true;
    }

    case ID_RENDER_CSM_DEBUG:
    {
        UINT state = GetMenuState(m_renderMenu, ID_RENDER_CSM_DEBUG, MF_BYCOMMAND);
        CheckMenuItem(m_renderMenu, ID_RENDER_CSM_DEBUG,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_csmDebugToggleCallback) m_csmDebugToggleCallback();
        return true;
    }

    case ID_OPTIM_PCSS:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_PCSS, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_PCSS,
            MF_BYCOMMAND | ((state & MF_CHECKED) ? MF_UNCHECKED : MF_CHECKED));
        if (m_pcssToggleCallback) m_pcssToggleCallback();
        return true;
    }

    case ID_OPTIM_PCSS_SIZE_SMALL:
    case ID_OPTIM_PCSS_SIZE_NORMAL:
    case ID_OPTIM_PCSS_SIZE_LARGE:
    {
        CheckMenuRadioItem(m_optimMenu,
            ID_OPTIM_PCSS_SIZE_SMALL, ID_OPTIM_PCSS_SIZE_LARGE, id, MF_BYCOMMAND);
        if (m_pcssLightSizeCallback)
        {
            float mult = (id == ID_OPTIM_PCSS_SIZE_SMALL) ? 0.5f
                       : (id == ID_OPTIM_PCSS_SIZE_LARGE) ? 2.0f : 1.0f;
            m_pcssLightSizeCallback(mult);
        }
        return true;
    }

    case ID_OPTIM_TORCH_SHADOW_0:
    case ID_OPTIM_TORCH_SHADOW_1:
    case ID_OPTIM_TORCH_SHADOW_2:
    case ID_OPTIM_TORCH_SHADOW_3:
    case ID_OPTIM_TORCH_SHADOW_4:
        CheckMenuRadioItem(m_optimMenu,
            ID_OPTIM_TORCH_SHADOW_0, ID_OPTIM_TORCH_SHADOW_4, id, MF_BYCOMMAND);
        if (m_torchShadowCountCallback)
            m_torchShadowCountCallback(static_cast<int>(id - ID_OPTIM_TORCH_SHADOW_0));
        return true;

    // Animation commands (Phase 34 Part A)
    case ID_ANIM_PLAY_PAUSE:
        if (m_animPlayPauseCallback) m_animPlayPauseCallback();
        return true;

    case ID_ANIM_STOP:
        if (m_animStopCallback) m_animStopCallback();
        return true;

    case ID_ANIM_SPEED_HALF:
    case ID_ANIM_SPEED_NORMAL:
    case ID_ANIM_SPEED_DOUBLE:
        CheckMenuRadioItem(m_animMenu, ID_ANIM_SPEED_HALF, ID_ANIM_SPEED_DOUBLE, id, MF_BYCOMMAND);
        if (m_animSpeedCallback)
        {
            float speed = (id == ID_ANIM_SPEED_HALF) ? 0.5f
                        : (id == ID_ANIM_SPEED_DOUBLE) ? 2.0f : 1.0f;
            m_animSpeedCallback(speed);
        }
        return true;

    default:
        // Dynamic animation clip selection
        if (id >= ID_ANIM_CLIP_BASE && id < ID_ANIM_CLIP_BASE + MAX_ANIM_CLIPS)
        {
            size_t clipIndex = static_cast<size_t>(id - ID_ANIM_CLIP_BASE);
            if (clipIndex < m_animClipCount)
            {
                CheckMenuRadioItem(m_animMenu,
                    ID_ANIM_CLIP_BASE,
                    static_cast<UINT>(ID_ANIM_CLIP_BASE + m_animClipCount - 1),
                    id, MF_BYCOMMAND);
                if (m_animClipSelectCallback) m_animClipSelectCallback(clipIndex);
            }
            return true;
        }
        return false;
    }
}

void Win32Menu::SetAnimationClips(const std::vector<std::string>& clipNames)
{
    if (!m_animMenu) return;

    // Remove all items from index 7 onward (after the separator following speed options)
    // Layout: Play(0), Stop(1), Sep(2), Half(3), Normal(4), Double(5), Sep(6), clips...
    constexpr int kClipStartPos = 7;
    int itemCount = GetMenuItemCount(m_animMenu);
    for (int i = itemCount - 1; i >= kClipStartPos; --i)
        DeleteMenu(m_animMenu, static_cast<UINT>(i), MF_BYPOSITION);

    m_animClipCount = 0;

    if (clipNames.empty())
    {
        AppendMenuW(m_animMenu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, L"(No Animation)");
        // Disable Play/Pause, Stop, speed items
        for (UINT id : {ID_ANIM_PLAY_PAUSE, ID_ANIM_STOP,
                        ID_ANIM_SPEED_HALF, ID_ANIM_SPEED_NORMAL, ID_ANIM_SPEED_DOUBLE})
            EnableMenuItem(m_animMenu, id, MF_BYCOMMAND | MF_GRAYED);
        return;
    }

    // Enable transport & speed controls
    for (UINT id : {ID_ANIM_PLAY_PAUSE, ID_ANIM_STOP,
                    ID_ANIM_SPEED_HALF, ID_ANIM_SPEED_NORMAL, ID_ANIM_SPEED_DOUBLE})
        EnableMenuItem(m_animMenu, id, MF_BYCOMMAND | MF_ENABLED);
    CheckMenuRadioItem(m_animMenu, ID_ANIM_SPEED_HALF, ID_ANIM_SPEED_DOUBLE,
        ID_ANIM_SPEED_NORMAL, MF_BYCOMMAND);

    size_t count = (clipNames.size() < MAX_ANIM_CLIPS) ? clipNames.size() : MAX_ANIM_CLIPS;
    m_animClipCount = count;

    for (size_t i = 0; i < count; ++i)
    {
        // Convert clip name to wide string
        const std::string& name = clipNames[i];
        int wLen = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, nullptr, 0);
        std::wstring wName(wLen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wName.data(), wLen);

        AppendMenuW(m_animMenu, MF_STRING,
            static_cast<UINT>(ID_ANIM_CLIP_BASE + i), wName.c_str());
    }

    // Default-check the first clip
    if (count > 0)
        CheckMenuRadioItem(m_animMenu,
            ID_ANIM_CLIP_BASE,
            static_cast<UINT>(ID_ANIM_CLIP_BASE + count - 1),
            ID_ANIM_CLIP_BASE, MF_BYCOMMAND);

    // Force menu bar redraw
    DrawMenuBar(m_hwnd);
}

void Win32Menu::SetTorchShadowMenuEnabled(bool enabled, int checkedCount)
{
    UINT flag = enabled ? MF_ENABLED : MF_GRAYED;
    for (UINT id = ID_OPTIM_TORCH_SHADOW_0; id <= ID_OPTIM_TORCH_SHADOW_4; id++)
        EnableMenuItem(m_optimMenu, id, MF_BYCOMMAND | flag);
    if (enabled)
        CheckMenuRadioItem(m_optimMenu,
            ID_OPTIM_TORCH_SHADOW_0, ID_OPTIM_TORCH_SHADOW_4,
            ID_OPTIM_TORCH_SHADOW_0 + checkedCount, MF_BYCOMMAND);
}

} // namespace RRE
