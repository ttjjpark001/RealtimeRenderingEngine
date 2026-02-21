#include "Platform/Win32/Win32Window.h"
#include "Platform/Win32/Win32Menu.h"

namespace RRE
{

Win32Window::Win32Window() = default;

Win32Window::~Win32Window()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hInstance)
    {
        UnregisterClassW(WINDOW_CLASS_NAME, m_hInstance);
    }
}

bool Win32Window::Initialize(uint32 width, uint32 height, const std::string& title, HINSTANCE hInstance)
{
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        return false;
    }

    // Calculate window rect for desired client area size
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, style, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center window on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    // Convert title to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    std::wstring wideTitle(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wideTitle.data(), wideLen);

    m_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        wideTitle.c_str(),
        style,
        posX, posY,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        m_hInstance,
        this
    );

    if (!m_hwnd)
    {
        return false;
    }

    m_isRunning = true;
    return true;
}

void Win32Window::ProcessMessages()
{
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            m_isRunning = false;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Win32Window::Show(int nCmdShow)
{
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

void Win32Window::SetWindowed(uint32 width, uint32 height)
{
    m_isFullscreen = false;

    DWORD style = WS_OVERLAPPEDWINDOW;
    SetWindowLongPtr(m_hwnd, GWL_STYLE, style);

    BOOL hasMenu = (::GetMenu(m_hwnd) != nullptr) ? TRUE : FALSE;
    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, style, hasMenu);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center on screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    SetWindowPos(m_hwnd, HWND_TOP,
        posX, posY, windowWidth, windowHeight,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_width = width;
    m_height = height;
}

void Win32Window::SetFullscreen()
{
    if (m_isFullscreen)
        return;

    // Save current window state
    m_savedStyle = static_cast<DWORD>(GetWindowLongPtr(m_hwnd, GWL_STYLE));
    GetWindowRect(m_hwnd, &m_savedWindowRect);

    // Switch to borderless fullscreen
    DWORD style = WS_POPUP | WS_VISIBLE;
    SetWindowLongPtr(m_hwnd, GWL_STYLE, style);

    HMONITOR monitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(monitor, &monitorInfo);

    int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

    SetWindowPos(m_hwnd, HWND_TOP,
        monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
        monitorWidth, monitorHeight,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    m_isFullscreen = true;
    m_width = static_cast<uint32>(monitorWidth);
    m_height = static_cast<uint32>(monitorHeight);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Win32Window* window = nullptr;

    if (msg == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<Win32Window*>(createStruct->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->m_hwnd = hwnd;
    }
    else
    {
        window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window)
    {
        return window->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        uint32 newWidth = LOWORD(lParam);
        uint32 newHeight = HIWORD(lParam);
        if (newWidth > 0 && newHeight > 0)
        {
            m_width = newWidth;
            m_height = newHeight;
            if (m_resizeCallback)
            {
                m_resizeCallback(m_width, m_height);
            }
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (m_menu && m_menu->HandleCommand(wParam))
            return 0;
        break;
    }

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE && m_isFullscreen)
        {
            // Restore previous windowed mode
            SetWindowLongPtr(m_hwnd, GWL_STYLE, m_savedStyle);
            SetWindowPos(m_hwnd, HWND_TOP,
                m_savedWindowRect.left, m_savedWindowRect.top,
                m_savedWindowRect.right - m_savedWindowRect.left,
                m_savedWindowRect.bottom - m_savedWindowRect.top,
                SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            m_isFullscreen = false;

            // Update client area size
            RECT clientRect;
            GetClientRect(m_hwnd, &clientRect);
            m_width = static_cast<uint32>(clientRect.right - clientRect.left);
            m_height = static_cast<uint32>(clientRect.bottom - clientRect.top);

            if (m_resizeCallback)
            {
                m_resizeCallback(m_width, m_height);
            }
            return 0;
        }

        if (m_keyCallback)
        {
            m_keyCallback(wParam);
        }
        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        m_isRunning = false;
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

} // namespace RRE
