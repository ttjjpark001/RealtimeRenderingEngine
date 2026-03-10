#pragma once

#include "Core/Types.h"
#include <DirectXMath.h>

namespace RRE
{

class IRHIContext;

struct RenderStats
{
    float fps;
    uint32 width;
    uint32 height;
    float aspectRatio;
    uint32 totalPolygons    = 0;  // full scene polygon count (no culling/LOD)
    uint32 renderedPolygons = 0;  // actual polygons submitted after culling + LOD
    float polygonsPerSec    = 0.0f;

    // Scene node / mesh counts (Phase 22)
    uint32 totalNodes = 0;
    uint32 totalMeshNodes = 0;

    // Culling / LOD statistics (Phase 23)
    uint32 visibleNodes       = 0;
    uint32 frustumCulledNodes = 0;
    uint32 activeLights       = 0;
    uint32 culledLights       = 0;

    // Light info (Phase 9)
    bool showLightInfo = false;
    const char* lightColorName = "White";
    DirectX::XMFLOAT3 lightPosition = { 0.0f, 0.0f, 0.0f };

    // Camera info (Phase 10)
    bool showCameraInfo = false;
    const char* projectionModeName = "Perspective";
    DirectX::XMFLOAT3 cameraPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 cameraDirection = { 0.0f, 0.0f, 0.0f };
    float fovDegrees = 45.0f;

    // Render mode info (Phase 20)
    const char* renderModeName = "Full PBR + Shadows";

    // Texture streaming stats (Phase 27)
    uint64 vramUsedMB      = 0;
    uint64 vramBudgetMB    = 0;
    uint32 trackedTextures = 0;
};

class DebugHUD
{
public:
    DebugHUD() = default;
    ~DebugHUD() = default;

    void Update(float deltaTime, const RenderStats& stats);
    void Render(IRHIContext& context);

private:
    float m_fpsAccumulator = 0.0f;
    int m_frameCount = 0;
    float m_displayFPS = 0.0f;
    RenderStats m_lastStats = {};

    static constexpr float kFPSUpdateInterval = 0.5f;
};

} // namespace RRE
