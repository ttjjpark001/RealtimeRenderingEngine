#include "Asset/SceneLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stdexcept>
#include <cmath>

namespace RRE
{

struct SceneLoader::AssimpContext
{
    const aiScene* scene = nullptr;
    SceneData* sceneData = nullptr;
    // Mesh index mapping: aiScene mesh index -> our meshes vector index
    std::vector<uint32> meshIndexMap;
};

SceneData SceneLoader::LoadScene(const std::string& filePath)
{
    Assimp::Importer importer;

    const aiScene* aiScenePtr = importer.ReadFile(filePath,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs);

    if (!aiScenePtr || (aiScenePtr->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !aiScenePtr->mRootNode)
    {
        throw std::runtime_error("Assimp failed to load scene: " + std::string(importer.GetErrorString()));
    }

    SceneData data;

    // Convert all meshes
    AssimpContext ctx;
    ctx.scene = aiScenePtr;
    ctx.sceneData = &data;
    ctx.meshIndexMap.resize(aiScenePtr->mNumMeshes);

    for (unsigned int i = 0; i < aiScenePtr->mNumMeshes; ++i)
    {
        ctx.meshIndexMap[i] = static_cast<uint32>(data.meshes.size());
        data.meshes.push_back(ConvertMesh(aiScenePtr->mMeshes[i]));
    }

    // Build SceneNode tree
    data.rootNode = std::make_unique<SceneNode>();
    ProcessNode(ctx, aiScenePtr->mRootNode, data.rootNode.get());

    // Extract camera
    data.camera = ExtractCamera(aiScenePtr);

    // Calculate scene bounds
    CalculateSceneBounds(data);

    return data;
}

void SceneLoader::ProcessNode(AssimpContext& ctx, const void* aiNodePtr,
                              SceneNode* parentNode)
{
    const aiNode* node = static_cast<const aiNode*>(aiNodePtr);

    // Extract transform from aiNode
    const aiMatrix4x4& m = node->mTransformation;
    // Assimp uses row-major matrices, DirectXMath also row-major — direct copy
    DirectX::XMMATRIX localMatrix(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );

    // Decompose into TRS
    DirectX::XMVECTOR scale, rotQuat, translation;
    DirectX::XMMatrixDecompose(&scale, &rotQuat, &translation, localMatrix);

    DirectX::XMFLOAT3 pos, scl;
    DirectX::XMStoreFloat3(&pos, translation);
    DirectX::XMStoreFloat3(&scl, scale);

    // Convert quaternion to Euler angles
    DirectX::XMFLOAT4 quat;
    DirectX::XMStoreFloat4(&quat, rotQuat);
    // Euler from quaternion (YXZ order)
    float sinP = 2.0f * (quat.w * quat.x - quat.y * quat.z);
    float pitch = std::abs(sinP) >= 1.0f ? std::copysign(DirectX::XM_PIDIV2, sinP) : std::asin(sinP);
    float sinY = 2.0f * (quat.w * quat.y + quat.x * quat.z);
    float cosY = 1.0f - 2.0f * (quat.x * quat.x + quat.y * quat.y);
    float yaw = std::atan2(sinY, cosY);
    float sinR = 2.0f * (quat.w * quat.z + quat.x * quat.y);
    float cosR = 1.0f - 2.0f * (quat.x * quat.x + quat.z * quat.z);
    float roll = std::atan2(sinR, cosR);

    parentNode->GetTransform().SetPosition(pos);
    parentNode->GetTransform().SetRotation({ pitch, yaw, roll });
    parentNode->GetTransform().SetScale(scl);

    // Assign mesh if this node has one (use first mesh for simplicity)
    if (node->mNumMeshes > 0)
    {
        uint32 meshIdx = ctx.meshIndexMap[node->mMeshes[0]];
        parentNode->SetMesh(ctx.sceneData->meshes[meshIdx].get());
    }

    // If this node has multiple meshes, create child nodes for extras
    for (unsigned int i = 1; i < node->mNumMeshes; ++i)
    {
        auto childNode = std::make_unique<SceneNode>();
        uint32 meshIdx = ctx.meshIndexMap[node->mMeshes[i]];
        childNode->SetMesh(ctx.sceneData->meshes[meshIdx].get());
        parentNode->AddChild(std::move(childNode));
    }

    // Process children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        auto childNode = std::make_unique<SceneNode>();
        SceneNode* childPtr = parentNode->AddChild(std::move(childNode));
        ProcessNode(ctx, node->mChildren[i], childPtr);
    }
}

std::unique_ptr<Mesh> SceneLoader::ConvertMesh(const void* aiMeshPtr)
{
    const aiMesh* mesh = static_cast<const aiMesh*>(aiMeshPtr);
    auto result = std::make_unique<Mesh>();

    // Reserve space
    result->vertices.reserve(mesh->mNumVertices);
    result->indices.reserve(mesh->mNumFaces * 3);

    bool hasUVs = mesh->mTextureCoords[0] != nullptr;

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex{};

        // Position
        vertex.position = {
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        };

        // Normal
        if (mesh->mNormals)
        {
            vertex.normal = {
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            };
        }

        // Store UV in color for now (Phase 01 vertex format has color but no UV)
        // color.x = u, color.y = v, color.z = 0, color.w = 1
        if (hasUVs)
        {
            vertex.color = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y,
                0.0f,
                1.0f
            };
        }
        else
        {
            vertex.color = { 0.8f, 0.8f, 0.8f, 1.0f };  // Default gray
        }

        result->vertices.push_back(vertex);
    }

    // Indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            result->indices.push_back(static_cast<uint32>(face.mIndices[j]));
        }
    }

    return result;
}

std::optional<CameraInfo> SceneLoader::ExtractCamera(const void* aiScenePtr)
{
    const aiScene* scene = static_cast<const aiScene*>(aiScenePtr);

    if (scene->mNumCameras == 0)
        return std::nullopt;

    const aiCamera* cam = scene->mCameras[0];

    CameraInfo info;
    info.position = { cam->mPosition.x, cam->mPosition.y, cam->mPosition.z };

    // LookAt = position + direction
    aiVector3D lookDir = cam->mLookAt;
    info.lookAt = {
        cam->mPosition.x + lookDir.x,
        cam->mPosition.y + lookDir.y,
        cam->mPosition.z + lookDir.z
    };

    info.fovY = cam->mHorizontalFOV > 0.0f ? cam->mHorizontalFOV : DirectX::XM_PIDIV4;
    info.nearPlane = cam->mClipPlaneNear > 0.0f ? cam->mClipPlaneNear : 0.1f;
    info.farPlane = cam->mClipPlaneFar > 0.0f ? cam->mClipPlaneFar : 1000.0f;

    // Find the camera's node to get the world transform
    const aiNode* camNode = scene->mRootNode->FindNode(cam->mName);
    if (camNode)
    {
        // Accumulate parent transforms
        aiMatrix4x4 worldTransform;
        const aiNode* current = camNode;
        std::vector<aiMatrix4x4> chain;
        while (current)
        {
            chain.push_back(current->mTransformation);
            current = current->mParent;
        }
        aiMatrix4x4 accumulated;
        for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i)
        {
            accumulated *= chain[i];
        }

        // Transform position and lookAt
        aiVector3D worldPos = accumulated * cam->mPosition;
        aiVector3D worldLookAt = accumulated * (cam->mPosition + cam->mLookAt);

        info.position = { worldPos.x, worldPos.y, worldPos.z };
        info.lookAt = { worldLookAt.x, worldLookAt.y, worldLookAt.z };
    }

    return info;
}

void SceneLoader::CalculateSceneBounds(SceneData& data)
{
    for (const auto& mesh : data.meshes)
    {
        for (const auto& vertex : mesh->vertices)
        {
            data.sceneBounds.Expand(vertex.position);
        }
    }
}

} // namespace RRE
