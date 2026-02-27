#include "Asset/Texture.h"

#include <cstring>
#include <memory>

namespace RRE
{

bool Texture::CreateFromData(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                              const uint8* pixels, uint32 width, uint32 height,
                              DXGI_FORMAT format)
{
    if (!device || !cmdList || !pixels || width == 0 || height == 0)
        return false;

    m_width = width;
    m_height = height;
    m_format = format;

    const uint32 bytesPerPixel = 4; // RGBA
    const uint32 rowPitch = width * bytesPerPixel;

    // Create DEFAULT heap texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeapProps = {};
    defaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr))
        return false;

    // Calculate required upload buffer size
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    uint64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, nullptr, nullptr, &totalBytes);

    // Create UPLOAD heap buffer
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer));

    if (FAILED(hr))
        return false;

    // Copy pixel data into upload buffer (row by row, respecting pitch alignment)
    uint8* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    hr = m_uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
    if (FAILED(hr))
        return false;

    uint8* dst = mapped + footprint.Offset;
    for (uint32 row = 0; row < height; ++row)
    {
        memcpy(dst + row * footprint.Footprint.RowPitch,
               pixels + row * rowPitch,
               rowPitch);
    }
    m_uploadBuffer->Unmap(0, nullptr);

    // Copy from upload buffer to texture resource
    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = m_uploadBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = m_resource.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // Transition from COPY_DEST to PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
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

} // namespace RRE
