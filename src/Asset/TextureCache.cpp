#include "Asset/TextureCache.h"
#include "RHI/D3D12/D3D12DescriptorHeap.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8   // Use _wfopen_s internally so Unicode (Korean etc.) paths work
#include <stb_image.h>

#include <algorithm>
#include <string>

namespace
{
    // Returns true if path ends with ".dds" (case-insensitive)
    bool IsDDSPath(const std::string& path)
    {
        if (path.size() < 4)
            return false;
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(::tolower(c)); });
        return ext == ".dds";
    }

    // Convert UTF-8 string to wide string for DirectXTex
    std::wstring ToWide(const std::string& utf8)
    {
        if (utf8.empty())
            return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        std::wstring result(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), len);
        return result;
    }
} // anonymous namespace

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

    // Save persistent descriptor index after fallback so Clear() can recycle slots on scene change
    if (m_srvHeap)
        m_basePersistentIndex = m_srvHeap->GetPersistentIndex();

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

    std::unique_ptr<Texture> texture;

    if (IsDDSPath(path))
    {
        // DDS: use DirectXTex (supports BC1/BC3/BC5/BC7 and mipmaps)
        std::wstring wpath = ToWide(path);
        texture = Texture::CreateFromDDS(device, cmdList, wpath.c_str(), isSRGB);
        if (!texture)
            return m_fallbackTexture.get();
    }
    else
    {
        // Non-DDS: decode with stb_image
        int width = 0, height = 0, channels = 0;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels)
        {
            // stb_image failed. Scenes like Amazon Bistro ship with DDS-only textures while
            // the glTF (via MSFT_texture_dds extension) references PNG paths that don't exist.
            // Try replacing the extension with ".dds" and loading via DirectXTex.
            size_t dotPos = path.find_last_of('.');
            size_t slashPos = path.find_last_of("/\\");
            bool hasExtAfterSlash = (dotPos != std::string::npos) &&
                                    (slashPos == std::string::npos || dotPos > slashPos);
            if (hasExtAfterSlash)
            {
                std::string ddsPath = path.substr(0, dotPos) + ".dds";
                std::wstring wDdsPath = ToWide(ddsPath);
                texture = Texture::CreateFromDDS(device, cmdList, wDdsPath.c_str(), isSRGB);
            }
            if (!texture)
                return m_fallbackTexture.get();
        }
        else
        {
            DXGI_FORMAT format = isSRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                                        : DXGI_FORMAT_R8G8B8A8_UNORM;

            texture = std::make_unique<Texture>();
            bool success = texture->CreateFromData(device, cmdList, pixels,
                                                    static_cast<uint32>(width),
                                                    static_cast<uint32>(height),
                                                    format);
            stbi_image_free(pixels);

            if (!success)
                return m_fallbackTexture.get();
        }
    }

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
    srvDesc.Texture2D.MipLevels = texture->GetMipLevels();
    srvDesc.Texture2D.MostDetailedMip = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE srvCpu = m_srvHeap->AllocatePersistent();
    device->CreateShaderResourceView(texture->GetResource(), &srvDesc, srvCpu);

    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = m_srvHeap->GetGPUHandleForCPU(srvCpu);
    texture->SetSRVHandles(srvCpu, srvGpu);
}

void TextureCache::Clear()
{
    m_cache.clear();
    // Recycle persistent descriptor slots used by old textures (fallback slots remain)
    if (m_srvHeap && m_basePersistentIndex > 0)
        m_srvHeap->SetPersistentIndex(m_basePersistentIndex);
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
