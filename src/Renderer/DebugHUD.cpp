#include "Renderer/DebugHUD.h"
#include "RHI/RHIContext.h"
#include <cstdio>

namespace RRE
{

void DebugHUD::Update(float deltaTime, const RenderStats& stats)
{
    m_lastStats = stats;

    m_fpsAccumulator += deltaTime;
    m_frameCount++;

    if (m_fpsAccumulator >= kFPSUpdateInterval)
    {
        m_displayFPS = static_cast<float>(m_frameCount) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_frameCount = 0;
    }
}

void DebugHUD::Render(IRHIContext& context)
{
    const DirectX::XMFLOAT4 green = { 0.0f, 1.0f, 0.0f, 1.0f };

    char buf[128];
    int y = 10;
    constexpr int x = 10;
    constexpr int lineHeight = 20;

    snprintf(buf, sizeof(buf), "FPS: %.1f", m_displayFPS);
    context.DrawText(x, y, buf, green);
    y += lineHeight;

    snprintf(buf, sizeof(buf), "Resolution: %ux%u", m_lastStats.width, m_lastStats.height);
    context.DrawText(x, y, buf, green);
    y += lineHeight;

    // Determine aspect ratio label
    float ar = m_lastStats.aspectRatio;
    const char* arLabel = "";
    if (ar > 1.76f && ar < 1.78f) arLabel = " (16:9)";
    else if (ar > 1.59f && ar < 1.61f) arLabel = " (16:10)";
    else if (ar > 1.32f && ar < 1.34f) arLabel = " (4:3)";

    snprintf(buf, sizeof(buf), "Aspect Ratio: %.2f%s", ar, arLabel);
    context.DrawText(x, y, buf, green);
    y += lineHeight;

    snprintf(buf, sizeof(buf), "Polygons: %u", m_lastStats.totalPolygons);
    context.DrawText(x, y, buf, green);
    y += lineHeight;

    float polyPerSecM = m_lastStats.polygonsPerSec / 1000000.0f;
    snprintf(buf, sizeof(buf), "Poly/sec: %.1fM", polyPerSecM);
    context.DrawText(x, y, buf, green);
    y += lineHeight;

    // Light info (conditional)
    if (m_lastStats.showLightInfo)
    {
        snprintf(buf, sizeof(buf), "Light: %s", m_lastStats.lightColorName);
        context.DrawText(x, y, buf, green);
        y += lineHeight;

        snprintf(buf, sizeof(buf), "Light Pos: (%.1f, %.1f, %.1f)",
            m_lastStats.lightPosition.x, m_lastStats.lightPosition.y, m_lastStats.lightPosition.z);
        context.DrawText(x, y, buf, green);
    }
}

} // namespace RRE
