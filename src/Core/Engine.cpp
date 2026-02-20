#include "Core/Engine.h"
#include "Platform/Win32/Win32Window.h"

namespace RRE
{

Engine::Engine() = default;

Engine::~Engine()
{
    Shutdown();
}

bool Engine::Initialize(const EngineInitParams& params)
{
    HINSTANCE hInstance = static_cast<HINSTANCE>(params.platformHandle);

    // Create window
    m_window = std::make_unique<Win32Window>();
    if (!m_window->Initialize(960, 540, "Realtime Rendering Engine", hInstance))
    {
        return false;
    }
    m_window->Show(params.showCommand);

    // Initialize high-resolution timer
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    m_timerFrequency = freq.QuadPart;

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    m_lastFrameTime = counter.QuadPart;

    m_isInitialized = true;
    return true;
}

void Engine::Run()
{
    while (m_window->IsRunning())
    {
        m_window->ProcessMessages();

        // Calculate delta time
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        float deltaTime = static_cast<float>(currentTime.QuadPart - m_lastFrameTime)
            / static_cast<float>(m_timerFrequency);
        m_lastFrameTime = currentTime.QuadPart;

        Update(deltaTime);
        Render();
    }
}

void Engine::Shutdown()
{
    if (!m_isInitialized)
        return;

    m_window.reset();
    m_isInitialized = false;
}

void Engine::Update(float deltaTime)
{
    // TODO: Scene update logic
    UNREFERENCED_PARAMETER(deltaTime);
}

void Engine::Render()
{
    // TODO: RHI BeginFrame → Clear → EndFrame
}

} // namespace RRE
