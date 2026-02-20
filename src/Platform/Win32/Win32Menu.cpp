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
