#include "Core/AnimationController.h"
#include "Scene/SceneNode.h"
#include <DirectXMath.h>
#include <unordered_map>

using namespace DirectX;

namespace RRE
{

void AnimationController::SetClip(const AnimationClip* clip)
{
    m_clip        = clip;
    m_currentTime = 0.0f;
    m_isPlaying   = false;
    m_bindPoses.clear();

    if (m_clip)
        CaptureBindPoses();
}

void AnimationController::SetClipByIndex(const std::vector<AnimationClip>& clips, size_t index)
{
    if (index < clips.size())
        SetClip(&clips[index]);
    else
        SetClip(nullptr);
}

void AnimationController::Play()
{
    if (m_clip)
        m_isPlaying = true;
}

void AnimationController::Pause()
{
    m_isPlaying = false;
}

void AnimationController::Stop()
{
    m_isPlaying   = false;
    m_currentTime = 0.0f;
}

void AnimationController::CaptureBindPoses()
{
    if (!m_clip) return;

    for (const auto& channel : m_clip->channels)
    {
        SceneNode* node = channel.targetNode;
        if (!node || m_bindPoses.count(node)) continue;

        XMMATRIX local = node->GetTransform().GetLocalMatrix();
        XMVECTOR scale, rotQ, trans;
        BindPoseTRS bp;
        if (XMMatrixDecompose(&scale, &rotQ, &trans, local))
        {
            XMStoreFloat3(&bp.translation, trans);
            XMStoreFloat4(&bp.rotation,    rotQ);
            XMStoreFloat3(&bp.scale,       scale);
        }
        m_bindPoses[node] = bp;
    }
}

void AnimationController::Update(float dt)
{
    if (!m_isPlaying || !m_clip || m_clip->channels.empty())
        return;

    m_currentTime += dt * m_playbackSpeed;

    if (m_clip->duration > 0.0f)
    {
        while (m_currentTime >= m_clip->duration) m_currentTime -= m_clip->duration;
        while (m_currentTime <  0.0f)             m_currentTime += m_clip->duration;
    }

    // --- Per-node TRS accumulator ---
    struct NodeTRS
    {
        XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
        XMFLOAT4 rotation    = { 0.0f, 0.0f, 0.0f, 1.0f };
        XMFLOAT3 scale       = { 1.0f, 1.0f, 1.0f };
    };

    std::unordered_map<SceneNode*, NodeTRS> nodeTRS;

    // Pass 1: seed each node from its bind pose
    for (const auto& channel : m_clip->channels)
    {
        SceneNode* node = channel.targetNode;
        if (!node || nodeTRS.count(node)) continue;

        auto it = m_bindPoses.find(node);
        if (it != m_bindPoses.end())
        {
            NodeTRS trs;
            trs.translation = it->second.translation;
            trs.rotation    = it->second.rotation;
            trs.scale       = it->second.scale;
            nodeTRS[node]   = trs;
        }
        else
        {
            nodeTRS[node] = NodeTRS{};
        }
    }

    // Pass 2: override with animated channel values at current time
    for (const auto& channel : m_clip->channels)
    {
        SceneNode* node = channel.targetNode;
        if (!node) continue;
        auto& trs = nodeTRS[node];

        switch (channel.property)
        {
        case AnimationProperty::Translation:
            trs.translation = InterpolateFloat3(channel.posKeys, m_currentTime, channel.interpolation);
            break;
        case AnimationProperty::Rotation:
            trs.rotation = InterpolateQuaternion(channel.rotKeys, m_currentTime, channel.interpolation);
            break;
        case AnimationProperty::Scale:
            trs.scale = InterpolateFloat3(channel.posKeys, m_currentTime, channel.interpolation);
            break;
        }
    }

    // Pass 3: compose and apply TRS matrix to each node
    for (auto& [node, trs] : nodeTRS)
    {
        XMMATRIX S     = XMMatrixScaling(trs.scale.x, trs.scale.y, trs.scale.z);
        XMMATRIX R     = XMMatrixRotationQuaternion(XMLoadFloat4(&trs.rotation));
        XMMATRIX T     = XMMatrixTranslation(trs.translation.x, trs.translation.y, trs.translation.z);
        XMMATRIX local = S * R * T;
        node->GetTransform().SetLocalMatrix(local);
    }
}

} // namespace RRE
