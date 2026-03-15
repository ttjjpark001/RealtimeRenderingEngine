#pragma once

#include "Core/Types.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace RRE
{

// Tracks loaded textures, computes per-frame priority (visibility + camera distance),
// and monitors GPU VRAM budget via IDXGIAdapter3.
// Phase 27: infrastructure + VRAM monitoring.  Async mip streaming deferred to Phase 28+.
class TextureStreamer
{
public:
    struct VRAMStats
    {
        uint64 usedMB   = 0;    // current GPU local memory in use
        uint64 budgetMB = 0;    // OS-granted budget
        float  usageRatio = 0.0f; // usedMB / budgetMB (0~1)
    };

    struct StreamStats
    {
        uint32    trackedTextures = 0; // number of registered textures
        VRAMStats vram;
    };

    TextureStreamer()  = default;
    ~TextureStreamer() = default;

    TextureStreamer(const TextureStreamer&)            = delete;
    TextureStreamer& operator=(const TextureStreamer&) = delete;

    // Pass the hardware adapter obtained from D3D12Device::GetDXGIAdapter3().
    // May be nullptr (e.g. WARP adapter); in that case VRAM monitoring is skipped.
    void Initialize(IDXGIAdapter3* adapter);

    // Register a texture that has just been loaded into GPU memory.
    // Call once per texture after TextureCache::GetOrLoad() returns.
    void Register(const std::string& key);

    // Update priority for a texture based on this frame's visibility and camera distance.
    // priority = isVisible ? 1/(distance+1) : 0
    void SetPriority(const std::string& key, bool isVisible, float distanceFromCamera);

    // Query VRAM from DXGI adapter.
    // Typically called once every ~0.5 s from Engine::Update().
    void UpdateVRAM();

    // Returns true when current VRAM usage exceeds 80% of the OS-granted budget.
    // Engine can use this to throttle additional texture loads.
    bool IsVRAMOverBudget() const;

    // Snapshot of streaming state for DebugHUD.
    StreamStats GetStats() const;

    // Remove all tracked texture entries (call before scene change, after GPU idle).
    void Clear();

private:
    struct TexEntry
    {
        float priority  = 0.0f;
        bool  isVisible = false;
    };

    mutable std::mutex                        m_mutex;
    std::unordered_map<std::string, TexEntry> m_entries;
    Microsoft::WRL::ComPtr<IDXGIAdapter3>     m_adapter;
    VRAMStats                                 m_vramStats;
};

} // namespace RRE
