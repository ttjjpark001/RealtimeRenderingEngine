#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include "Core/Types.h"

namespace RRE
{

class Win32Menu;

class Win32Window
{
public:
    using ResizeCallback = std::function<void(uint32, uint32)>;
    using KeyCallback = std::function<void(WPARAM)>;

    Win32Window();
    ~Win32Window();

    bool Initialize(uint32 width, uint32 height, const std::string& title, HINSTANCE hInstance);
    void ProcessMessages();
    bool IsRunning() const { return m_isRunning; }

    uint32 GetWidth() const { return m_width; }
    uint32 GetHeight() const { return m_height; }
    HWND GetHWND() const { return m_hwnd; }

    void SetWindowed(uint32 width, uint32 height);
    void SetFullscreen();
    bool IsFullscreen() const { return m_isFullscreen; }

    void SetResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }
    void SetKeyCallback(KeyCallback callback) { m_keyCallback = std::move(callback); }
    void SetMenu(Win32Menu* menu) { m_menu = menu; }

    void Show(int nCmdShow = SW_SHOW);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    uint32 m_width = 0;
    uint32 m_height = 0;
    bool m_isRunning = false;
    bool m_isFullscreen = false;

    // Saved window state for fullscreen restore
    RECT m_savedWindowRect = {};
    DWORD m_savedStyle = 0;

    ResizeCallback m_resizeCallback;
    KeyCallback m_keyCallback;
    Win32Menu* m_menu = nullptr;

    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"RREngineWindowClass";
};

} // namespace RRE
