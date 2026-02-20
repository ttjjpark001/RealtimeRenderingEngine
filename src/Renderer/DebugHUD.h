#pragma once

#include "Core/Types.h"

namespace RRE
{

class IRHIContext;

struct RenderStats
{
    float fps;
    uint32 width;
    uint32 height;
    float aspectRatio;
    uint32 totalPolygons;
    float polygonsPerSec;
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
