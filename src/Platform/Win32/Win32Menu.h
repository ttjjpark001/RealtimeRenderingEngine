#pragma once

#include <Windows.h>
#include <functional>
#include "Core/Types.h"

namespace RRE
{

// Menu command IDs
constexpr UINT ID_VIEW_800x450      = 1001;
constexpr UINT ID_VIEW_960x540      = 1002;
constexpr UINT ID_VIEW_FULLSCREEN   = 1003;

constexpr UINT ID_LIGHT_SHOW_INFO   = 4001;
constexpr UINT ID_LIGHT_WHITE       = 4002;
constexpr UINT ID_LIGHT_RED         = 4003;
constexpr UINT ID_LIGHT_GREEN       = 4004;
constexpr UINT ID_LIGHT_BLUE        = 4005;
constexpr UINT ID_LIGHT_YELLOW      = 4006;
constexpr UINT ID_LIGHT_CYAN        = 4007;
constexpr UINT ID_LIGHT_MAGENTA     = 4008;

constexpr UINT ID_CAMERA_SHOW_INFO    = 5001;
constexpr UINT ID_CAMERA_PERSPECTIVE  = 5002;
constexpr UINT ID_CAMERA_ORTHOGRAPHIC = 5003;
constexpr UINT ID_CAMERA_FOV_UP       = 5004;
constexpr UINT ID_CAMERA_FOV_DOWN     = 5005;
constexpr UINT ID_CAMERA_RESET        = 5006;
constexpr UINT ID_CAMERA_FIT_TO_SCENE = 5007;

constexpr UINT ID_FILE_OPEN_SCENE     = 6001;
constexpr UINT ID_FILE_OPEN_SPONZA    = 6002;
constexpr UINT ID_FILE_OPEN_BISTRO    = 6003;

constexpr UINT ID_RENDER_WIREFRAME       = 7001;
constexpr UINT ID_RENDER_SOLID           = 7002;
constexpr UINT ID_RENDER_BASECOLOR       = 7003;
constexpr UINT ID_RENDER_FULL_PBR        = 7004;
constexpr UINT ID_RENDER_FULL_PBR_SHADOW = 7005;

constexpr UINT ID_OPTIM_LOD              = 8001;
constexpr UINT ID_OPTIM_FRUSTUM_CULL     = 8002;
constexpr UINT ID_OPTIM_LIGHT_CULL       = 8003;
constexpr UINT ID_OPTIM_MIPMAP           = 8004;
constexpr UINT ID_OPTIM_OCCLUSION_CULL   = 8005;
constexpr UINT ID_OPTIM_CSM              = 8006;
constexpr UINT ID_OPTIM_CSM_DEBUG        = 8007;
constexpr UINT ID_OPTIM_PCSS             = 8008;
constexpr UINT ID_OPTIM_PCSS_SIZE_SMALL  = 8009;
constexpr UINT ID_OPTIM_PCSS_SIZE_NORMAL = 8010;
constexpr UINT ID_OPTIM_PCSS_SIZE_LARGE  = 8011;

class Win32Menu
{
public:
    using ViewCallback = std::function<void(uint32, uint32, bool)>;
    using LightColorCallback = std::function<void(float, float, float)>;
    using LightToggleInfoCallback = std::function<void()>;
    using CameraProjectionCallback = std::function<void(bool perspective)>;
    using CameraToggleInfoCallback = std::function<void()>;
    using CameraFovCallback = std::function<void(float deltaDegrees)>;
    using CameraResetCallback = std::function<void()>;
    using FileOpenCallback   = std::function<void()>;
    using FileSponzaCallback = std::function<void()>;
    using FileBistroCallback = std::function<void()>;
    using CameraFitToSceneCallback = std::function<void()>;
    using RenderModeCallback = std::function<void(uint32 mode)>;
    using LODToggleCallback              = std::function<void()>;
    using FrustumCullToggleCallback      = std::function<void()>;
    using LightCullToggleCallback        = std::function<void()>;
    using MipMapToggleCallback           = std::function<void()>;
    using OcclusionCullToggleCallback    = std::function<void()>;
    using CSMToggleCallback              = std::function<void()>;
    using CSMDebugToggleCallback         = std::function<void()>;
    using PCSSToggleCallback             = std::function<void()>;
    using PCSSLightSizeCallback          = std::function<void(float multiplier)>;

    Win32Menu() = default;
    ~Win32Menu() = default;

    bool Initialize(HWND hwnd);

    bool HandleCommand(WPARAM wParam);

    void SetViewCallback(ViewCallback callback) { m_viewCallback = std::move(callback); }
    void SetLightColorCallback(LightColorCallback callback) { m_lightColorCallback = std::move(callback); }
    void SetLightToggleInfoCallback(LightToggleInfoCallback callback) { m_lightToggleInfoCallback = std::move(callback); }
    void SetCameraProjectionCallback(CameraProjectionCallback callback) { m_cameraProjectionCallback = std::move(callback); }
    void SetCameraToggleInfoCallback(CameraToggleInfoCallback callback) { m_cameraToggleInfoCallback = std::move(callback); }
    void SetCameraFovCallback(CameraFovCallback callback) { m_cameraFovCallback = std::move(callback); }
    void SetCameraResetCallback(CameraResetCallback callback) { m_cameraResetCallback = std::move(callback); }
    void SetFileOpenCallback(FileOpenCallback callback)     { m_fileOpenCallback   = std::move(callback); }
    void SetFileSponzaCallback(FileSponzaCallback callback) { m_fileSponzaCallback = std::move(callback); }
    void SetFileBistroCallback(FileBistroCallback callback) { m_fileBistroCallback = std::move(callback); }
    void SetCameraFitToSceneCallback(CameraFitToSceneCallback callback) { m_cameraFitToSceneCallback = std::move(callback); }
    void SetRenderModeCallback(RenderModeCallback callback) { m_renderModeCallback = std::move(callback); }
    void SetLODToggleCallback(LODToggleCallback callback)             { m_lodToggleCallback = std::move(callback); }
    void SetFrustumCullToggleCallback(FrustumCullToggleCallback cb)   { m_frustumCullToggleCallback = std::move(cb); }
    void SetLightCullToggleCallback(LightCullToggleCallback cb)       { m_lightCullToggleCallback = std::move(cb); }
    void SetMipMapToggleCallback(MipMapToggleCallback cb)             { m_mipMapToggleCallback = std::move(cb); }
    void SetOcclusionCullToggleCallback(OcclusionCullToggleCallback cb) { m_occlusionCullToggleCallback = std::move(cb); }
    void SetCSMToggleCallback(CSMToggleCallback cb)                     { m_csmToggleCallback = std::move(cb); }
    void SetCSMDebugToggleCallback(CSMDebugToggleCallback cb)           { m_csmDebugToggleCallback = std::move(cb); }
    void SetPCSSToggleCallback(PCSSToggleCallback cb)                   { m_pcssToggleCallback = std::move(cb); }
    void SetPCSSLightSizeCallback(PCSSLightSizeCallback cb)             { m_pcssLightSizeCallback = std::move(cb); }

private:
    HWND m_hwnd = nullptr;
    HMENU m_menuBar = nullptr;
    HMENU m_fileMenu = nullptr;
    HMENU m_viewMenu = nullptr;
    HMENU m_lightMenu = nullptr;
    HMENU m_cameraMenu = nullptr;
    HMENU m_renderMenu = nullptr;
    HMENU m_optimMenu = nullptr;

    ViewCallback m_viewCallback;
    LightColorCallback m_lightColorCallback;
    LightToggleInfoCallback m_lightToggleInfoCallback;
    CameraProjectionCallback m_cameraProjectionCallback;
    CameraToggleInfoCallback m_cameraToggleInfoCallback;
    CameraFovCallback m_cameraFovCallback;
    CameraResetCallback m_cameraResetCallback;
    FileOpenCallback   m_fileOpenCallback;
    FileSponzaCallback m_fileSponzaCallback;
    FileBistroCallback m_fileBistroCallback;
    CameraFitToSceneCallback m_cameraFitToSceneCallback;
    RenderModeCallback m_renderModeCallback;
    LODToggleCallback              m_lodToggleCallback;
    FrustumCullToggleCallback      m_frustumCullToggleCallback;
    LightCullToggleCallback        m_lightCullToggleCallback;
    MipMapToggleCallback           m_mipMapToggleCallback;
    OcclusionCullToggleCallback    m_occlusionCullToggleCallback;
    CSMToggleCallback              m_csmToggleCallback;
    CSMDebugToggleCallback         m_csmDebugToggleCallback;
    PCSSToggleCallback             m_pcssToggleCallback;
    PCSSLightSizeCallback          m_pcssLightSizeCallback;
};

} // namespace RRE
