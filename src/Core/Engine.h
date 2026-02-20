#pragma once

#include "Core/Types.h"
#include <memory>

namespace RRE
{

class Win32Window;
class Win32Menu;
class IRHIDevice;
class IRHIBuffer;
class Mesh;
class DebugHUD;
class PointLight;
enum class MeshType;

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

    void OnViewModeChanged(uint32 width, uint32 height, bool fullscreen);
    void OnMeshTypeChanged(MeshType type);
    void OnAnimationToggle();

    std::unique_ptr<Win32Window> m_window;
    std::unique_ptr<Win32Menu> m_menu;
    std::unique_ptr<IRHIDevice> m_rhiDevice;

    // Mesh buffers
    std::unique_ptr<IRHIBuffer> m_vertexBuffer;
    std::unique_ptr<IRHIBuffer> m_indexBuffer;
    uint32 m_indexCount = 0;

    // Meshes (all 4 types pre-created)
    std::unique_ptr<Mesh> m_sphereMesh;
    std::unique_ptr<Mesh> m_tetrahedronMesh;
    std::unique_ptr<Mesh> m_cubeMesh;
    std::unique_ptr<Mesh> m_cylinderMesh;
    Mesh* m_currentMesh = nullptr;

    // Debug HUD
    std::unique_ptr<DebugHUD> m_debugHUD;

    // Point light
    std::unique_ptr<PointLight> m_pointLight;
    bool m_showLightInfo = true;

    // Animation
    float m_rotationAngle = 0.0f;
    bool m_isAnimating = true;

    bool m_isInitialized = false;

    // High-resolution timer
    int64 m_timerFrequency = 0;
    int64 m_lastFrameTime = 0;
};

} // namespace RRE
