#include "Asset/TextureCache.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace RRE
{

bool TextureCache::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                               D3D12DescriptorHeap* srvHeap)
{
    m_device = device;
    m_srvHeap = srvHeap;

    // glTF UV origin (top-left) matches D3D12, so no vertical flip needed.
    // stbi loads row 0 = top of image, which is correct for D3D12 textures.
    stbi_set_flip_vertically_on_load(false);

    m_fallbackTexture = Texture::CreateFallback(device, cmdList);
    if (!m_fallbackTexture)
        return false;

    // Create SRV for fallback texture
    CreateSRV(device, m_fallbackTexture.get());

    return true;
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

    // Create SRV for the loaded texture
    CreateSRV(device, texture.get());

    Texture* result = texture.get();
    m_cache[path] = std::move(texture);
    return result;
}

Texture* TextureCache::GetOrLoadFromMemory(const std::string& key, const uint8_t* data, size_t dataSize,
                                            bool isSRGB, ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (!data || dataSize == 0)
        return m_fallbackTexture.get();

    // Check cache first
    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second.get();

    // Decode image from memory with stb_image
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(data, static_cast<int>(dataSize),
                                             &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        return m_fallbackTexture.get();

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

    CreateSRV(device, texture.get());

    Texture* result = texture.get();
    m_cache[key] = std::move(texture);
    return result;
}

Texture* TextureCache::GetOrLoadFromRawPixels(const std::string& key, const uint8_t* pixels,
                                                uint32 width, uint32 height, bool isSRGB,
                                                ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (!pixels || width == 0 || height == 0)
        return m_fallbackTexture.get();

    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second.get();

    DXGI_FORMAT format = isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                : DXGI_FORMAT_R8G8B8A8_UNORM;

    auto texture = std::make_unique<Texture>();
    bool success = texture->CreateFromData(device, cmdList, pixels, width, height, format);

    if (!success)
        return m_fallbackTexture.get();

    CreateSRV(device, texture.get());

    Texture* result = texture.get();
    m_cache[key] = std::move(texture);
    return result;
}

void TextureCache::CreateSRV(ID3D12Device* device, Texture* texture)
{
    if (!m_srvHeap || !device || !texture || !texture->GetResource())
        return;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = texture->GetFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = m_srvHeap->AllocatePersistent();
    device->CreateShaderResourceView(texture->GetResource(), &srvDesc, srvCpu);

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = m_srvHeap->GetGPUHandleForCPU(srvCpu);
    texture->SetSRVHandles(srvCpu, srvGpu);
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
