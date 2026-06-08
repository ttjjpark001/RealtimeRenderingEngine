#include "Asset/SceneLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>

#include <stdexcept>
#include <cmath>
#include <filesystem>


namespace RRE
{

struct SceneLoader::AssimpContext
{
    const aiScene* scene = nullptr;
    SceneData* sceneData = nullptr;
    std::string sceneDir;
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
        aiProcess_ConvertToLeftHanded);

    if (!aiScenePtr || (aiScenePtr->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !aiScenePtr->mRootNode)
    {
        throw std::runtime_error("Assimp failed to load scene: " + std::string(importer.GetErrorString()));
    }

    SceneData data;
    // u8path preserves UTF-8 encoding (e.g. Korean username in path) then converts back to UTF-8 string
    std::string sceneDir = std::filesystem::u8path(filePath).parent_path().u8string();

    // Convert all materials
    for (unsigned int i = 0; i < aiScenePtr->mNumMaterials; ++i)
    {
        data.materials.push_back(ConvertMaterial(aiScenePtr->mMaterials[i], aiScenePtr, sceneDir, data));
    }

    // Convert all meshes
    AssimpContext ctx;
    ctx.scene = aiScenePtr;
    ctx.sceneData = &data;
    ctx.sceneDir = sceneDir;
    ctx.meshIndexMap.resize(aiScenePtr->mNumMeshes);

    for (unsigned int i = 0; i < aiScenePtr->mNumMeshes; ++i)
    {
        ctx.meshIndexMap[i] = static_cast<uint32>(data.meshes.size());
        auto mesh = ConvertMesh(aiScenePtr->mMeshes[i]);
        // Link material index
        int32 matIdx = static_cast<int32>(aiScenePtr->mMeshes[i]->mMaterialIndex);
        if (matIdx >= 0 && matIdx < static_cast<int32>(data.materials.size()))
            mesh->materialIndex = matIdx;
        data.meshes.push_back(std::move(mesh));
    }

    // Build SceneNode tree
    data.rootNode = std::make_unique<SceneNode>();
    ProcessNode(ctx, aiScenePtr->mRootNode, data.rootNode.get());

    // Extract camera
    data.camera = ExtractCamera(aiScenePtr);

    // Calculate scene bounds
    CalculateSceneBounds(data);

    // Load animations (Phase 34 Part A)
    LoadAnimations(aiScenePtr, data.rootNode.get(), data);

    return data;
}

void SceneLoader::ProcessNode(AssimpContext& ctx, const void* aiNodePtr,
                              SceneNode* parentNode)
{
    const aiNode* node = static_cast<const aiNode*>(aiNodePtr);

    // Set node name for animation channel targeting
    parentNode->SetName(node->mName.C_Str());

    // Extract transform from aiNode
    const aiMatrix4x4& m = node->mTransformation;
    // Assimp uses column-vector convention (v' = M * v), translation in column 4
    // DirectXMath uses row-vector convention (v' = v * M), translation in row 4
    // Must transpose when converting between conventions
    DirectX::XMMATRIX localMatrix(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );

    // Store the matrix directly to avoid lossy TRS decompose→recompose round-trip.
    // Euler angle extraction and recomposition can use mismatched rotation orders,
    // causing geometry distortion (mirroring) for models with non-trivial transforms.
    parentNode->GetTransform().SetLocalMatrix(localMatrix);

    // Primitive splitting: each aiMesh becomes a separate SceneNode
    // - Single mesh: assign directly to parentNode (avoids an extra empty node level)
    // - Multiple meshes: ALL become individual child nodes so parentNode is a transform container
    //   This ensures every mesh has its own SceneNode with a dedicated AABB → enables per-node Culling/LOD
    if (node->mNumMeshes == 1)
    {
        uint32 meshIdx = ctx.meshIndexMap[node->mMeshes[0]];
        Mesh* mesh = ctx.sceneData->meshes[meshIdx].get();
        parentNode->SetMesh(mesh);
        if (mesh->materialIndex >= 0 &&
            mesh->materialIndex < static_cast<int32>(ctx.sceneData->materials.size()))
        {
            parentNode->SetMaterial(ctx.sceneData->materials[mesh->materialIndex].get());
        }
    }
    else if (node->mNumMeshes > 1)
    {
        // All meshes become children; parentNode holds only the transform
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            auto childNode = std::make_unique<SceneNode>();
            uint32 meshIdx = ctx.meshIndexMap[node->mMeshes[i]];
            Mesh* mesh = ctx.sceneData->meshes[meshIdx].get();
            childNode->SetMesh(mesh);
            if (mesh->materialIndex >= 0 &&
                mesh->materialIndex < static_cast<int32>(ctx.sceneData->materials.size()))
            {
                childNode->SetMaterial(ctx.sceneData->materials[mesh->materialIndex].get());
            }
            parentNode->AddChild(std::move(childNode));
        }
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
    bool hasTangents = mesh->mTangents != nullptr;

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

        // Default gray color for loaded meshes (Material system handles appearance)
        vertex.color = { 0.8f, 0.8f, 0.8f, 1.0f };

        // UV coordinates
        if (hasUVs)
        {
            vertex.texCoord = {
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            };
        }
        else
        {
            vertex.texCoord = { 0.0f, 0.0f };
        }

        // Tangent (with handedness in w)
        if (hasTangents)
        {
            // Compute handedness from bitangent
            float handedness = 1.0f;
            if (mesh->mBitangents)
            {
                DirectX::XMVECTOR n = DirectX::XMLoadFloat3(&vertex.normal);
                DirectX::XMVECTOR t = DirectX::XMVectorSet(
                    mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z, 0.0f);
                DirectX::XMVECTOR b = DirectX::XMVectorSet(
                    mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z, 0.0f);
                DirectX::XMVECTOR computedB = DirectX::XMVector3Cross(n, t);
                float dot;
                DirectX::XMStoreFloat(&dot, DirectX::XMVector3Dot(computedB, b));
                handedness = dot < 0.0f ? -1.0f : 1.0f;
            }
            vertex.tangent = {
                mesh->mTangents[i].x,
                mesh->mTangents[i].y,
                mesh->mTangents[i].z,
                handedness
            };
        }
        else
        {
            vertex.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
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

    // Compute local-space AABB
    if (!result->vertices.empty())
    {
        std::vector<DirectX::XMFLOAT3> positions;
        positions.reserve(result->vertices.size());
        for (const auto& v : result->vertices)
            positions.push_back(v.position);
        DirectX::BoundingBox::CreateFromPoints(
            result->aabb,
            positions.size(),
            positions.data(),
            sizeof(DirectX::XMFLOAT3));
    }

    return result;
}

std::unique_ptr<Material> SceneLoader::ConvertMaterial(const void* aiMaterialPtr,
                                                       const void* aiScenePtr,
                                                       const std::string& sceneDir,
                                                       SceneData& sceneData)
{
    const aiMaterial* mat = static_cast<const aiMaterial*>(aiMaterialPtr);
    const aiScene* scene = static_cast<const aiScene*>(aiScenePtr);
    auto result = std::make_unique<Material>();

    // Base color factor
    aiColor4D baseColor;
    if (mat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS)
    {
        result->baseColorFactor = { baseColor.r, baseColor.g, baseColor.b, baseColor.a };
    }
    else
    {
        // Fallback: try diffuse color
        aiColor4D diffuse;
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS)
        {
            result->baseColorFactor = { diffuse.r, diffuse.g, diffuse.b, diffuse.a };
        }
    }

    // Metallic factor (default 1.0 per glTF spec, always assign)
    float metallic = 1.0f;
    mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
    result->metallicFactor = metallic;

    // Roughness factor (default 1.0 per glTF spec, always assign)
    float roughness = 1.0f;
    mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
    result->roughnessFactor = roughness;

    // Emissive factor
    aiColor3D emissive;
    if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS)
    {
        result->emissiveFactor = { emissive.r, emissive.g, emissive.b };
    }

    // Alpha mode
    aiString alphaMode;
    if (mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS)
    {
        std::string mode(alphaMode.C_Str());
        if (mode == "MASK")
            result->alphaMode = AlphaMode::Mask;
        else if (mode == "BLEND")
            result->alphaMode = AlphaMode::Blend;
        else
            result->alphaMode = AlphaMode::Opaque;
    }

    // Alpha cutoff
    float alphaCutoff = 0.5f;
    if (mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff) == AI_SUCCESS)
    {
        result->alphaCutoff = alphaCutoff;
    }

    // Double-sided
    int twoSided = 0;
    if (mat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
    {
        result->doubleSided = (twoSided != 0);
    }

    // Texture paths (handles both file-based and embedded textures)
    auto extractTexturePath = [&](aiTextureType type) -> std::string
    {
        if (mat->GetTextureCount(type) > 0)
        {
            aiString path;
            if (mat->GetTexture(type, 0, &path) == AI_SUCCESS)
            {
                std::string texPath(path.C_Str());

                // Check for embedded texture (path starts with '*')
                if (!texPath.empty() && texPath[0] == '*')
                {
                    // Already extracted? skip duplicate work
                    if (sceneData.embeddedTextures.find(texPath) == sceneData.embeddedTextures.end())
                    {
                        int texIndex = std::atoi(texPath.c_str() + 1);
                        if (texIndex >= 0 && static_cast<unsigned int>(texIndex) < scene->mNumTextures)
                        {
                            const aiTexture* aiTex = scene->mTextures[texIndex];
                            EmbeddedTextureData embedded;

                            if (aiTex->mHeight == 0)
                            {
                                // Compressed format (PNG, JPG, etc.) — mWidth is data size in bytes
                                embedded.isCompressed = true;
                                embedded.width = aiTex->mWidth;
                                embedded.height = 0;
                                embedded.data.resize(aiTex->mWidth);
                                std::memcpy(embedded.data.data(), aiTex->pcData, aiTex->mWidth);
                            }
                            else
                            {
                                // Raw ARGB data — convert to RGBA
                                embedded.isCompressed = false;
                                embedded.width = aiTex->mWidth;
                                embedded.height = aiTex->mHeight;
                                size_t pixelCount = aiTex->mWidth * aiTex->mHeight;
                                embedded.data.resize(pixelCount * 4);
                                for (size_t p = 0; p < pixelCount; ++p)
                                {
                                    embedded.data[p * 4 + 0] = aiTex->pcData[p].r;
                                    embedded.data[p * 4 + 1] = aiTex->pcData[p].g;
                                    embedded.data[p * 4 + 2] = aiTex->pcData[p].b;
                                    embedded.data[p * 4 + 3] = aiTex->pcData[p].a;
                                }
                            }

                            sceneData.embeddedTextures[texPath] = std::move(embedded);
                        }
                    }
                    return texPath;
                }

                // File-based: if path is relative, prepend scene directory
                if (!texPath.empty() && texPath[0] != '/' && texPath[0] != '\\' &&
                    (texPath.size() < 2 || texPath[1] != ':'))
                {
                    texPath = sceneDir + "/" + texPath;
                }
                return texPath;
            }
        }
        return "";
    };

    result->baseColorTexturePath = extractTexturePath(aiTextureType_BASE_COLOR);
    if (result->baseColorTexturePath.empty())
        result->baseColorTexturePath = extractTexturePath(aiTextureType_DIFFUSE);

    result->normalTexturePath = extractTexturePath(aiTextureType_NORMALS);
    if (result->normalTexturePath.empty())
        result->normalTexturePath = extractTexturePath(aiTextureType_HEIGHT);
    if (result->normalTexturePath.empty())
        result->normalTexturePath = extractTexturePath(aiTextureType_NORMAL_CAMERA);

    // glTF metallic-roughness: Assimp maps to UNKNOWN (older) or DIFFUSE_ROUGHNESS/METALNESS (newer)
    result->metallicRoughnessTexturePath = extractTexturePath(aiTextureType_UNKNOWN);
    if (result->metallicRoughnessTexturePath.empty())
        result->metallicRoughnessTexturePath = extractTexturePath(aiTextureType_DIFFUSE_ROUGHNESS);
    if (result->metallicRoughnessTexturePath.empty())
        result->metallicRoughnessTexturePath = extractTexturePath(aiTextureType_METALNESS);

    result->emissiveTexturePath = extractTexturePath(aiTextureType_EMISSIVE);

    // Occlusion: glTF standard maps to AMBIENT_OCCLUSION, some exporters use LIGHTMAP
    result->occlusionTexturePath = extractTexturePath(aiTextureType_AMBIENT_OCCLUSION);
    if (result->occlusionTexturePath.empty())
        result->occlusionTexturePath = extractTexturePath(aiTextureType_LIGHTMAP);

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

// ---------------------------------------------------------------------------
// Phase 34 Part A: Node Transform Animation loading
// ---------------------------------------------------------------------------

namespace {

// Depth-first search for a SceneNode with the given name
SceneNode* FindNodeInTree(SceneNode* node, const std::string& name)
{
    if (!node) return nullptr;
    if (node->GetName() == name) return node;
    for (auto& child : node->GetChildren())
    {
        SceneNode* found = FindNodeInTree(child.get(), name);
        if (found) return found;
    }
    return nullptr;
}

AnimationInterpolation ConvertInterpolation(aiAnimInterpolation ai)
{
    switch (ai)
    {
    case aiAnimInterpolation_Step:         return AnimationInterpolation::Step;
    case aiAnimInterpolation_Cubic_Spline: return AnimationInterpolation::CubicSpline;
    default:                               return AnimationInterpolation::Linear;
    }
}

} // anonymous namespace

void SceneLoader::LoadAnimations(const void* aiScenePtr, SceneNode* rootNode, SceneData& data)
{
    const aiScene* scene = static_cast<const aiScene*>(aiScenePtr);
    if (!scene || !scene->HasAnimations()) return;

    constexpr double kDefaultTps = 25.0;

    for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai)
    {
        const aiAnimation* anim = scene->mAnimations[ai];
        const double tps = (anim->mTicksPerSecond > 0.0) ? anim->mTicksPerSecond : kDefaultTps;

        AnimationClip clip;
        clip.name     = anim->mName.C_Str();
        if (clip.name.empty())
            clip.name = "Clip_" + std::to_string(ai);
        clip.duration = static_cast<float>(anim->mDuration / tps);

        for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci)
        {
            const aiNodeAnim* nodeAnim = anim->mChannels[ci];

            SceneNode* target = FindNodeInTree(rootNode, nodeAnim->mNodeName.C_Str());
            if (!target) continue;

            // Translation channel
            if (nodeAnim->mNumPositionKeys > 0)
            {
                AnimationChannel ch;
                ch.targetNode    = target;
                ch.property      = AnimationProperty::Translation;
                ch.interpolation = ConvertInterpolation(
                    nodeAnim->mPositionKeys[0].mInterpolation);

                ch.posKeys.reserve(nodeAnim->mNumPositionKeys);
                for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; ++k)
                {
                    Keyframe<DirectX::XMFLOAT3> kf;
                    kf.time  = static_cast<float>(nodeAnim->mPositionKeys[k].mTime / tps);
                    kf.value = {
                        nodeAnim->mPositionKeys[k].mValue.x,
                        nodeAnim->mPositionKeys[k].mValue.y,
                        nodeAnim->mPositionKeys[k].mValue.z
                    };
                    ch.posKeys.push_back(kf);
                }
                clip.channels.push_back(std::move(ch));
            }

            // Rotation channel — aiQuaternion is (w,x,y,z), XMFLOAT4 is (x,y,z,w)
            if (nodeAnim->mNumRotationKeys > 0)
            {
                AnimationChannel ch;
                ch.targetNode    = target;
                ch.property      = AnimationProperty::Rotation;
                ch.interpolation = ConvertInterpolation(
                    nodeAnim->mRotationKeys[0].mInterpolation);

                ch.rotKeys.reserve(nodeAnim->mNumRotationKeys);
                for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; ++k)
                {
                    Keyframe<DirectX::XMFLOAT4> kf;
                    kf.time  = static_cast<float>(nodeAnim->mRotationKeys[k].mTime / tps);
                    kf.value = {
                        nodeAnim->mRotationKeys[k].mValue.x,
                        nodeAnim->mRotationKeys[k].mValue.y,
                        nodeAnim->mRotationKeys[k].mValue.z,
                        nodeAnim->mRotationKeys[k].mValue.w
                    };
                    ch.rotKeys.push_back(kf);
                }
                clip.channels.push_back(std::move(ch));
            }

            // Scale channel
            if (nodeAnim->mNumScalingKeys > 0)
            {
                AnimationChannel ch;
                ch.targetNode    = target;
                ch.property      = AnimationProperty::Scale;
                ch.interpolation = ConvertInterpolation(
                    nodeAnim->mScalingKeys[0].mInterpolation);

                ch.posKeys.reserve(nodeAnim->mNumScalingKeys);
                for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; ++k)
                {
                    Keyframe<DirectX::XMFLOAT3> kf;
                    kf.time  = static_cast<float>(nodeAnim->mScalingKeys[k].mTime / tps);
                    kf.value = {
                        nodeAnim->mScalingKeys[k].mValue.x,
                        nodeAnim->mScalingKeys[k].mValue.y,
                        nodeAnim->mScalingKeys[k].mValue.z
                    };
                    ch.posKeys.push_back(kf);
                }
                clip.channels.push_back(std::move(ch));
            }
        }

        if (!clip.channels.empty())
            data.animations.push_back(std::move(clip));
    }
}

} // namespace RRE
