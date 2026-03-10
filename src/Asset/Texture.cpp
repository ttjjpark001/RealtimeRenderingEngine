#include "Asset/Texture.h"

#include <DirectXTex.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace
{
    // Generate the next mip level from src (RGBA8, srcW×srcH) using a 2×2 box filter.
    std::vector<uint8_t> BoxFilterDownscale(const uint8_t* src, uint32_t srcW, uint32_t srcH)
    {
        uint32_t dstW = (std::max)(1u, srcW / 2);
        uint32_t dstH = (std::max)(1u, srcH / 2);
        std::vector<uint8_t> dst(static_cast<size_t>(dstW) * dstH * 4, 0);

        for (uint32_t y = 0; y < dstH; ++y)
        {
            for (uint32_t x = 0; x < dstW; ++x)
            {
                uint32_t sx  = x * 2;
                uint32_t sy  = y * 2;
                uint32_t sx1 = (std::min)(sx + 1, srcW - 1);
                uint32_t sy1 = (std::min)(sy + 1, srcH - 1);

                for (uint32_t c = 0; c < 4; ++c)
                {
                    uint32_t val = src[(sy  * srcW + sx ) * 4 + c]
                                 + src[(sy  * srcW + sx1) * 4 + c]
                                 + src[(sy1 * srcW + sx ) * 4 + c]
                                 + src[(sy1 * srcW + sx1) * 4 + c];
                    dst[(y * dstW + x) * 4 + c] = static_cast<uint8_t>(val / 4);
                }
            }
        }
        return dst;
    }
} // anonymous namespace

namespace RRE
{

bool Texture::CreateFromData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                              const uint8* pixels, uint32 width, uint32 height,
                              DXGI_FORMAT format)
{
    if (!device || !cmdList || !pixels || width == 0 || height == 0)
        return false;

    m_width  = width;
    m_height = height;
    m_format = format;

    constexpr uint32 kBytesPerPixel = 4; // RGBA8

    // --- Build full mip chain on CPU using box filter ---
    uint32 mipCount = 1;
    {
        uint32 w = width, h = height;
        while (w > 1 || h > 1) { w = (std::max)(1u, w / 2); h = (std::max)(1u, h / 2); ++mipCount; }
    }
    m_mipLevels = static_cast<uint16>(mipCount);

    std::vector<std::vector<uint8_t>> mipData(mipCount);
    mipData[0].assign(pixels, pixels + static_cast<size_t>(width) * height * kBytesPerPixel);
    {
        uint32 srcW = width, srcH = height;
        for (uint32 mip = 1; mip < mipCount; ++mip)
        {
            mipData[mip] = BoxFilterDownscale(mipData[mip - 1].data(), srcW, srcH);
            srcW = (std::max)(1u, srcW / 2);
            srcH = (std::max)(1u, srcH / 2);
        }
    }

    // --- Create DEFAULT heap texture with all mip levels ---
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width            = width;
    texDesc.Height           = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels        = static_cast<UINT16>(mipCount);
    texDesc.Format           = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    // --- Query footprints for all mip levels ---
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipCount);
    std::vector<uint32> numRows(mipCount);
    std::vector<uint64> rowSizes(mipCount);
    uint64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, mipCount, 0,
        footprints.data(), numRows.data(), rowSizes.data(), &totalBytes);

    // --- Create UPLOAD heap buffer for all mips ---
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width            = totalBytes;
    bufferDesc.Height           = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels        = 1;
    bufferDesc.Format           = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE,
        &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_uploadBuffer));
    if (FAILED(hr))
        return false;

    // --- Map and copy all mip levels into upload buffer ---
    uint8* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    hr = m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
    if (FAILED(hr))
        return false;

    uint32 mipW = width, mipH = height;
    for (uint32 mip = 0; mip < mipCount; ++mip)
    {
        const uint8* src = mipData[mip].data();
        uint32 srcRowPitch = mipW * kBytesPerPixel;
        uint8* dst = mapped + footprints[mip].Offset;
        uint64 copySize = rowSizes[mip] < srcRowPitch ? rowSizes[mip] : srcRowPitch;
        for (uint32 row = 0; row < numRows[mip]; ++row)
        {
            memcpy(dst + row * footprints[mip].Footprint.RowPitch,
                   src + row * srcRowPitch,
                   static_cast<size_t>(copySize));
        }
        mipW = (std::max)(1u, mipW / 2);
        mipH = (std::max)(1u, mipH / 2);
    }
    m_uploadBuffer->Unmap(0, nullptr);

    // --- Issue CopyTextureRegion for each mip level ---
    for (uint32 mip = 0; mip < mipCount; ++mip)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource        = m_uploadBuffer.Get();
        srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint  = footprints[mip];

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource        = m_resource.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = mip;

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    }

    // --- Transition to PIXEL_SHADER_RESOURCE ---
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    m_state = TextureState::Ready;
    return true;
}

std::unique_ptr<Texture> Texture::CreateFallback(ID3D12Device* device,
                                                  ID3D12GraphicsCommandList* cmdList)
{
    // 1x1 white pixel (RGBA)
    const uint8 whitePixel[4] = { 255, 255, 255, 255 };

    auto texture = std::make_unique<Texture>();
    if (!texture->CreateFromData(device, cmdList, whitePixel, 1, 1,
                                  DXGI_FORMAT_R8G8B8A8_UNORM))
    {
        return nullptr;
    }
    return texture;
}

std::unique_ptr<Texture> Texture::CreateFromDDS(ID3D12Device* device,
                                                  ID3D12GraphicsCommandList* cmdList,
                                                  const wchar_t* path, bool isSRGB)
{
    if (!device || !cmdList || !path)
        return nullptr;

    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    HRESULT hr = DirectX::LoadFromDDSFile(path, DirectX::DDS_FLAGS_NONE, &metadata, image);
    if (FAILED(hr))
        return nullptr;

    // Apply sRGB variant if requested (BC1_UNORM → BC1_UNORM_SRGB, etc.)
    DXGI_FORMAT format = metadata.format;
    if (isSRGB)
        format = DirectX::MakeSRGB(format);

    uint16 mipCount = static_cast<uint16>(metadata.mipLevels);
    uint32 numSubresources = mipCount;

    // Create DEFAULT heap texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<uint64>(metadata.width);
    texDesc.Height = static_cast<uint32>(metadata.height);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = mipCount;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    hr = device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));
    if (FAILED(hr))
        return nullptr;

    // Query footprints for all mip levels
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(numSubresources);
    std::vector<uint32> numRows(numSubresources);
    std::vector<uint64> rowSizes(numSubresources);
    uint64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, numSubresources, 0,
        footprints.data(), numRows.data(), rowSizes.data(), &totalBytes);

    // Create UPLOAD heap buffer for all mips
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
    hr = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
    if (FAILED(hr))
        return nullptr;

    // Map and copy pixel data for every mip level
    uint8* mapped = nullptr;
    D3D12_RANGE readRange = {};
    hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
    if (FAILED(hr))
        return nullptr;

    for (uint32 mip = 0; mip < numSubresources; ++mip)
    {
        const DirectX::Image* img = image.GetImage(mip, 0, 0);
        if (!img)
            continue;

        uint8* dst = mapped + footprints[mip].Offset;
        const uint8* src = img->pixels;
        uint64 copySize = rowSizes[mip];
        for (uint32 row = 0; row < numRows[mip]; ++row)
        {
            memcpy(dst + row * footprints[mip].Footprint.RowPitch,
                   src + row * img->rowPitch,
                   copySize);
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    // Issue CopyTextureRegion for each mip
    for (uint32 mip = 0; mip < numSubresources; ++mip)
    {
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = uploadBuffer.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprints[mip];

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = resource.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = mip;

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    }

    // Transition from COPY_DEST to PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Assemble Texture object
    auto texture = std::make_unique<Texture>();
    texture->m_resource = std::move(resource);
    texture->m_uploadBuffer = std::move(uploadBuffer);
    texture->m_width = static_cast<uint32>(metadata.width);
    texture->m_height = static_cast<uint32>(metadata.height);
    texture->m_mipLevels = mipCount;
    texture->m_format = format;
    texture->m_state = TextureState::Ready;
    return texture;
}

} // namespace RRE
