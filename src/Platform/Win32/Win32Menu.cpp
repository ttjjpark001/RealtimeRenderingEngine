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
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_800x450, L"800 x 450");
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_960x540, L"960 x 540");
    AppendMenuW(m_viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_viewMenu, MF_STRING, ID_VIEW_FULLSCREEN, L"Full Screen");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_viewMenu), L"View");

    // Default check: 960x540
    CheckMenuRadioItem(m_viewMenu, ID_VIEW_800x450, ID_VIEW_960x540,
        ID_VIEW_960x540, MF_BYCOMMAND);

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
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_CSM_DEBUG,      L"CSM Debug View");
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_PCSS,           L"PCSS (Soft Shadows)");
    AppendMenuW(m_optimMenu, MF_SEPARATOR,           0,                       nullptr);
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_PCSS_SIZE_SMALL,  L"PCSS Light Size: Small (0.5x)");
    AppendMenuW(m_optimMenu, MF_STRING | MF_CHECKED, ID_OPTIM_PCSS_SIZE_NORMAL, L"PCSS Light Size: Normal (1.0x)");
    AppendMenuW(m_optimMenu, MF_STRING,              ID_OPTIM_PCSS_SIZE_LARGE,  L"PCSS Light Size: Large (2.0x)");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_optimMenu), L"Optimization");

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
    case ID_VIEW_800x450:
        CheckMenuRadioItem(m_viewMenu, ID_VIEW_800x450, ID_VIEW_960x540,
            ID_VIEW_800x450, MF_BYCOMMAND);
        if (m_viewCallback) m_viewCallback(800, 450, false);
        return true;

    case ID_VIEW_960x540:
        CheckMenuRadioItem(m_viewMenu, ID_VIEW_800x450, ID_VIEW_960x540,
            ID_VIEW_960x540, MF_BYCOMMAND);
        if (m_viewCallback) m_viewCallback(960, 540, false);
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

    case ID_OPTIM_CSM_DEBUG:
    {
        UINT state = GetMenuState(m_optimMenu, ID_OPTIM_CSM_DEBUG, MF_BYCOMMAND);
        CheckMenuItem(m_optimMenu, ID_OPTIM_CSM_DEBUG,
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

    default:
        return false;
    }
}

} // namespace RRE
