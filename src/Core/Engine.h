#pragma once

#include "Core/Types.h"
#include <memory>

namespace RRE
{

class Win32Window;
class IRHIDevice;
class IRHIBuffer;
class Mesh;
class DebugHUD;

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
    void OnResize(uint32 width, uint32 height);
    void UploadMesh(const Mesh& mesh);

    std::unique_ptr<Win32Window> m_window;
    std::unique_ptr<IRHIDevice> m_rhiDevice;

    // Mesh buffers
    std::unique_ptr<IRHIBuffer> m_vertexBuffer;
    std::unique_ptr<IRHIBuffer> m_indexBuffer;
    uint32 m_indexCount = 0;

    // Cube mesh
    std::unique_ptr<Mesh> m_cubeMesh;

    // Debug HUD
    std::unique_ptr<DebugHUD> m_debugHUD;

    // Animation
    float m_rotationAngle = 0.0f;

    bool m_isInitialized = false;

    // High-resolution timer
    int64 m_timerFrequency = 0;
    int64 m_lastFrameTime = 0;
};

} // namespace RRE
