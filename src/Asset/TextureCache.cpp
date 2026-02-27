#include "Asset/TextureCache.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace RRE
{

bool TextureCache::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    m_fallbackTexture = Texture::CreateFallback(device, cmdList);
    return m_fallbackTexture != nullptr;
}

Texture* TextureCache::GetOrLoad(const std::string& path, bool isSRGB,
                                  ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (path.empty())
        return m_fallbackTexture.get();

    // Check cache first
    auto it = m_cache.find(path);
    if (it != m_cache.end())
        return it->second.get();

    // Decode image with stb_image
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return m_fallbackTexture.get();

    // Choose format based on sRGB flag
    DXGI_FORMAT format = isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                : DXGI_FORMAT_R8G8B8A8_UNORM;

    auto texture = std::make_unique<Texture>();
    bool success = texture->CreateFromData(device, cmdList, pixels,
                                            static_cast<uint32>(width),
                                            static_cast<uint32>(height),
                                            format);
    stbi_image_free(pixels);

    if (!success)
        return m_fallbackTexture.get();

    Texture* result = texture.get();
    m_cache[path] = std::move(texture);
    return result;
}

void TextureCache::Clear()
{
    m_cache.clear();
}

void TextureCache::ReleaseUploadBuffers()
{
    if (m_fallbackTexture)
        m_fallbackTexture->ReleaseUploadBuffer();

    for (auto& [path, texture] : m_cache)
    {
        if (texture)
            texture->ReleaseUploadBuffer();
    }
}

} // namespace RRE
