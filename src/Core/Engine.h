#pragma once

#include "Core/Types.h"
#include <memory>

namespace RRE
{

class Win32Window;

struct EngineInitParams
{
    void* platformHandle = nullptr;  // HINSTANCE on Windows
    int showCommand = 0;             // nCmdShow on Windows
};

class Engine
{
public:
    Engine();
    ~Engine();

    bool Initialize(const EngineInitParams& params);
    void Run();
    void Shutdown();

private:
    void Update(float deltaTime);
    void Render();

    std::unique_ptr<Win32Window> m_window;
    bool m_isInitialized = false;

    // High-resolution timer
    int64 m_timerFrequency = 0;
    int64 m_lastFrameTime = 0;
};

} // namespace RRE
