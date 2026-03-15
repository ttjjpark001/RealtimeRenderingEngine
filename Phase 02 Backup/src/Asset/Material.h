#pragma once

#include "Core/Types.h"
#include <DirectXMath.h>
#include <string>

namespace RRE
{

class Texture;

enum class AlphaMode
{
    Opaque,
    Mask,
    Blend
};

class Material
{
public:
    Material() = default;
    ~Material() = default;

    // Texture references (nullptr until Texture system is implemented in Phase 14)
    Texture* baseColorTexture = nullptr;
    Texture* metallicRoughnessTexture = nullptr;
    Texture* normalTexture = nullptr;
    Texture* emissiveTexture = nullptr;
    Texture* occlusionTexture = nullptr;

    // Texture file paths (extracted by SceneLoader, loaded in Phase 14)
    std::string baseColorTexturePath;
    std::string metallicRoughnessTexturePath;
    std::string normalTexturePath;
    std::string emissiveTexturePath;
    std::string occlusionTexturePath;

    // PBR factor values
    DirectX::XMFLOAT4 baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    DirectX::XMFLOAT3 emissiveFactor = { 0.0f, 0.0f, 0.0f };

    // Render state
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;

    // Dirty flag
    void SetDirty() { m_dirty = true; }
    bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }

    // Convenience setters that auto-set dirty
    void SetBaseColorFactor(const DirectX::XMFLOAT4& value) { baseColorFactor = value; SetDirty(); }
    void SetMetallicFactor(float value) { metallicFactor = value; SetDirty(); }
    void SetRoughnessFactor(float value) { roughnessFactor = value; SetDirty(); }
    void SetEmissiveFactor(const DirectX::XMFLOAT3& value) { emissiveFactor = value; SetDirty(); }
    void SetAlphaMode(AlphaMode mode) { alphaMode = mode; SetDirty(); }
    void SetAlphaCutoff(float value) { alphaCutoff = value; SetDirty(); }
    void SetDoubleSided(bool value) { doubleSided = value; SetDirty(); }

private:
    bool m_dirty = true;
};

} // namespace RRE
