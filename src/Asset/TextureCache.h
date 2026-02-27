#pragma once

#include "Asset/Texture.h"

#include <string>
#include <unordered_map>
#include <memory>

namespace RRE
{

class TextureCache
{
public:
    TextureCache() = default;
    ~TextureCache() = default;

    // Non-copyable
    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    // Initialize with a 1x1 white fallback texture
    bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    // Get cached texture or load from file path
    // isSRGB: true for baseColor (albedo), false for normal/metallic/roughness etc.
    Texture* GetOrLoad(const std::string& path, bool isSRGB,
                       ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

    // Get the fallback (1x1 white) texture
    Texture* GetFallback() const { return m_fallbackTexture.get(); }

    // Clear all cached textures (scene change)
    void Clear();

    // Release upload buffers after GPU copy is confirmed complete
    void ReleaseUploadBuffers();

    // Number of cached textures (excluding fallback)
    size_t GetCachedCount() const { return m_cache.size(); }

private:
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_cache;
    std::unique_ptr<Texture> m_fallbackTexture;
};

} // namespace RRE
