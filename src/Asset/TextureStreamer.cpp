#include "Asset/TextureStreamer.h"

namespace RRE
{

void TextureStreamer::Initialize(IDXGIAdapter3* adapter)
{
    m_adapter = adapter;
    UpdateVRAM();
}

void TextureStreamer::Register(const std::string& key)
{
    if (key.empty())
        return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.emplace(key, TexEntry{});
}

void TextureStreamer::SetPriority(const std::string& key, bool isVisible, float distanceFromCamera)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(key);
    if (it != m_entries.end())
    {
        it->second.isVisible = isVisible;
        it->second.priority  = isVisible ? (1.0f / (distanceFromCamera + 1.0f)) : 0.0f;
    }
}

void TextureStreamer::UpdateVRAM()
{
    if (!m_adapter)
        return;

    DXGI_QUERY_VIDEO_MEMORY_INFO memInfo = {};
    if (SUCCEEDED(m_adapter->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
    {
        m_vramStats.usedMB   = memInfo.CurrentUsage / (1024ull * 1024ull);
        m_vramStats.budgetMB = memInfo.Budget        / (1024ull * 1024ull);
        m_vramStats.usageRatio = (memInfo.Budget > 0)
            ? static_cast<float>(memInfo.CurrentUsage) / static_cast<float>(memInfo.Budget)
            : 0.0f;
    }
}

bool TextureStreamer::IsVRAMOverBudget() const
{
    return m_vramStats.usageRatio > 0.80f;
}

TextureStreamer::StreamStats TextureStreamer::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    StreamStats s;
    s.trackedTextures = static_cast<uint32>(m_entries.size());
    s.vram            = m_vramStats;
    return s;
}

void TextureStreamer::Clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

} // namespace RRE
