#pragma once

#include "Asset/Animation.h"
#include <DirectXMath.h>
#include <unordered_map>
#include <vector>

namespace RRE
{

class SceneNode;

class AnimationController
{
public:
    AnimationController() = default;
    ~AnimationController() = default;

    void SetClip(const AnimationClip* clip);
    void SetClipByIndex(const std::vector<AnimationClip>& clips, size_t index);

    void Play();
    void Pause();
    void Stop();

    void SetPlaybackSpeed(float speed) { m_playbackSpeed = speed; }
    float GetPlaybackSpeed() const     { return m_playbackSpeed; }

    bool  IsPlaying()     const { return m_isPlaying; }
    float GetCurrentTime() const { return m_currentTime; }
    float GetDuration()   const { return m_clip ? m_clip->duration : 0.0f; }
    const AnimationClip* GetCurrentClip() const { return m_clip; }

    void Update(float dt);

private:
    struct BindPoseTRS
    {
        DirectX::XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rotation    = { 0.0f, 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 scale       = { 1.0f, 1.0f, 1.0f };
    };

    void CaptureBindPoses();

    const AnimationClip* m_clip          = nullptr;
    float                m_currentTime   = 0.0f;
    float                m_playbackSpeed = 1.0f;
    bool                 m_isPlaying     = false;

    std::unordered_map<SceneNode*, BindPoseTRS> m_bindPoses;
};

} // namespace RRE
