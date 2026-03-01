#pragma once

#include "Core/Types.h"
#include "Asset/Material.h"
#include "Renderer/Mesh.h"
#include "Scene/SceneNode.h"
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cmath>

namespace RRE
{

struct BoundingBox
{
    DirectX::XMFLOAT3 min = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
    DirectX::XMFLOAT3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    bool IsValid() const
    {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    DirectX::XMFLOAT3 GetCenter() const
    {
        return {
            (min.x + max.x) * 0.5f,
            (min.y + max.y) * 0.5f,
            (min.z + max.z) * 0.5f
        };
    }

    float GetDiagonalLength() const
    {
        float dx = max.x - min.x;
        float dy = max.y - min.y;
        float dz = max.z - min.z;
        return std::sqrtf(dx * dx + dy * dy + dz * dz);
    }

    void Expand(const DirectX::XMFLOAT3& point)
    {
        if (point.x < min.x) min.x = point.x;
        if (point.y < min.y) min.y = point.y;
        if (point.z < min.z) min.z = point.z;
        if (point.x > max.x) max.x = point.x;
        if (point.y > max.y) max.y = point.y;
        if (point.z > max.z) max.z = point.z;
    }
};

struct CameraInfo
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 lookAt   = { 0.0f, 0.0f, 1.0f };
    float fovY = DirectX::XM_PIDIV4;  // 45 degrees
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct EmbeddedTextureData
{
    std::vector<uint8_t> data;
    bool isCompressed = false;  // true: PNG/JPG (stbi_load_from_memory), false: raw RGBA
    uint32_t width = 0, height = 0;  // raw일 때만 유효
};

struct SceneData
{
    std::vector<std::unique_ptr<Mesh>> meshes;
    std::vector<std::unique_ptr<Material>> materials;
    std::unique_ptr<SceneNode> rootNode;
    BoundingBox sceneBounds;
    std::optional<CameraInfo> camera;
    std::unordered_map<std::string, EmbeddedTextureData> embeddedTextures;
};

class SceneLoader
{
public:
    SceneLoader() = default;
    ~SceneLoader() = default;

    SceneData LoadScene(const std::string& filePath);

private:
    struct AssimpContext;

    void ProcessNode(AssimpContext& ctx, const void* aiNodePtr,
                     SceneNode* parentNode);
    std::unique_ptr<Mesh> ConvertMesh(const void* aiMeshPtr);
    std::unique_ptr<Material> ConvertMaterial(const void* aiMaterialPtr,
                                              const void* aiScenePtr,
                                              const std::string& sceneDir,
                                              SceneData& sceneData);
    std::optional<CameraInfo> ExtractCamera(const void* aiScenePtr);
    void CalculateSceneBounds(SceneData& data);
};

} // namespace RRE
