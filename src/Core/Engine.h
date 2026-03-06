#pragma once

#include "Core/Types.h"
#include <memory>
#include <vector>
#include <string>

namespace RRE
{

class Win32Window;
class Win32Menu;
class IRHIDevice;
class IRHIBuffer;
class Mesh;
class Material;
class DebugHUD;
class LightManager;
class Camera;
class Renderer;
class SceneGraph;
class TextureCache;

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

    void OnViewModeChanged(uint32 width, uint32 height, bool fullscreen);

    // Scene loading
    void LoadScene(const std::string& filePath);
    void ShowOpenSceneDialog();
    void LoadSponzaScene();

    std::unique_ptr<Win32Window> m_window;
    std::unique_ptr<Win32Menu> m_menu;
    std::unique_ptr<IRHIDevice> m_rhiDevice;

    // Renderer & Scene Graph
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<SceneGraph> m_sceneGraph;

    // Debug HUD
    std::unique_ptr<DebugHUD> m_debugHUD;

    // Light manager
    std::unique_ptr<LightManager> m_lightManager;
    bool m_showLightInfo = true;

    // Camera
    std::unique_ptr<Camera> m_camera;
    bool m_showCameraInfo = true;

    // Texture cache
    std::unique_ptr<TextureCache> m_textureCache;

    // Loaded scene data
    std::vector<std::unique_ptr<Mesh>> m_loadedMeshes;
    std::vector<std::unique_ptr<Material>> m_loadedMaterials;
    float m_sceneDiagonal = 10.0f;

    // Orbiting light (for PBR visualization)
    size_t m_orbitLightIndex = 0;
    float m_orbitLightAngle = 0.0f;

    // Sponza-specific sun direction toggle (L key)
    bool m_isSponzaScene = false;
    bool m_sponzaSunAltMode = false;
    bool m_sponzaSunToggleKeyWasDown = false;
    size_t m_sponzaSunKeyIndex = 0;  // index of sun (Key) light in m_lightManager

    bool m_isInitialized = false;

    // High-resolution timer
    int64 m_timerFrequency = 0;
    int64 m_lastFrameTime = 0;
};

} // namespace RRE
