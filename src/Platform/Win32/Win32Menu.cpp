#include "Platform/Win32/Win32Menu.h"

namespace RRE
{

bool Win32Menu::Initialize(HWND hwnd)
{
    m_hwnd = hwnd;

    m_menuBar = CreateMenu();
    if (!m_menuBar)
        return false;

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

    // Object menu
    m_objectMenu = CreatePopupMenu();
    AppendMenuW(m_objectMenu, MF_STRING, ID_OBJECT_SPHERE, L"Sphere");
    AppendMenuW(m_objectMenu, MF_STRING, ID_OBJECT_TETRAHEDRON, L"Tetrahedron");
    AppendMenuW(m_objectMenu, MF_STRING, ID_OBJECT_CUBE, L"Cube");
    AppendMenuW(m_objectMenu, MF_STRING, ID_OBJECT_CYLINDER, L"Cylinder");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_objectMenu), L"Object");

    // Default check: Cube
    CheckMenuRadioItem(m_objectMenu, ID_OBJECT_SPHERE, ID_OBJECT_CYLINDER,
        ID_OBJECT_CUBE, MF_BYCOMMAND);

    // Animation menu
    m_animMenu = CreatePopupMenu();
    AppendMenuW(m_animMenu, MF_STRING, ID_ANIM_PLAY, L"Play");
    AppendMenuW(m_animMenu, MF_STRING, ID_ANIM_PAUSE, L"Pause");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_animMenu), L"Animation");

    // Default check: Play
    CheckMenuRadioItem(m_animMenu, ID_ANIM_PLAY, ID_ANIM_PAUSE,
        ID_ANIM_PLAY, MF_BYCOMMAND);

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
    AppendMenuW(m_lightMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m_lightMenu, MF_STRING, ID_LIGHT_RESET_POS, L"Reset Position");
    AppendMenuW(m_menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(m_lightMenu), L"Light");

    // Default check: White
    CheckMenuRadioItem(m_lightMenu, ID_LIGHT_WHITE, ID_LIGHT_MAGENTA,
        ID_LIGHT_WHITE, MF_BYCOMMAND);

    SetMenu(hwnd, m_menuBar);
    return true;
}

bool Win32Menu::HandleCommand(WPARAM wParam)
{
    UINT id = LOWORD(wParam);

    switch (id)
    {
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

    // Object commands
    case ID_OBJECT_SPHERE:
        CheckMenuRadioItem(m_objectMenu, ID_OBJECT_SPHERE, ID_OBJECT_CYLINDER,
            ID_OBJECT_SPHERE, MF_BYCOMMAND);
        if (m_meshCallback) m_meshCallback(MeshType::Sphere);
        return true;

    case ID_OBJECT_TETRAHEDRON:
        CheckMenuRadioItem(m_objectMenu, ID_OBJECT_SPHERE, ID_OBJECT_CYLINDER,
            ID_OBJECT_TETRAHEDRON, MF_BYCOMMAND);
        if (m_meshCallback) m_meshCallback(MeshType::Tetrahedron);
        return true;

    case ID_OBJECT_CUBE:
        CheckMenuRadioItem(m_objectMenu, ID_OBJECT_SPHERE, ID_OBJECT_CYLINDER,
            ID_OBJECT_CUBE, MF_BYCOMMAND);
        if (m_meshCallback) m_meshCallback(MeshType::Cube);
        return true;

    case ID_OBJECT_CYLINDER:
        CheckMenuRadioItem(m_objectMenu, ID_OBJECT_SPHERE, ID_OBJECT_CYLINDER,
            ID_OBJECT_CYLINDER, MF_BYCOMMAND);
        if (m_meshCallback) m_meshCallback(MeshType::Cylinder);
        return true;

    // Animation commands
    case ID_ANIM_PLAY:
        CheckMenuRadioItem(m_animMenu, ID_ANIM_PLAY, ID_ANIM_PAUSE,
            ID_ANIM_PLAY, MF_BYCOMMAND);
        if (m_animCallback) m_animCallback();
        return true;

    case ID_ANIM_PAUSE:
        CheckMenuRadioItem(m_animMenu, ID_ANIM_PLAY, ID_ANIM_PAUSE,
            ID_ANIM_PAUSE, MF_BYCOMMAND);
        if (m_animCallback) m_animCallback();
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

    case ID_LIGHT_RESET_POS:
        if (m_lightResetCallback) m_lightResetCallback();
        return true;

    default:
        return false;
    }
}

void Win32Menu::UpdateAnimCheckMark(bool isPlaying)
{
    CheckMenuRadioItem(m_animMenu, ID_ANIM_PLAY, ID_ANIM_PAUSE,
        isPlaying ? ID_ANIM_PLAY : ID_ANIM_PAUSE, MF_BYCOMMAND);
}

} // namespace RRE
