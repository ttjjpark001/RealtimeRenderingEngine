#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

namespace RRE
{

class SceneNode;

enum class AnimationInterpolation { Linear, Step, CubicSpline };
enum class AnimationProperty     { Translation, Rotation, Scale };

template<typename T>
struct Keyframe
{
    float time  = 0.0f;
    T     value = {};
};

struct AnimationChannel
{
    SceneNode*             targetNode    = nullptr;
    AnimationProperty      property      = AnimationProperty::Translation;
    AnimationInterpolation interpolation = AnimationInterpolation::Linear;
    std::vector<Keyframe<DirectX::XMFLOAT3>> posKeys;  // Translation or Scale
    std::vector<Keyframe<DirectX::XMFLOAT4>> rotKeys;  // Rotation (quaternion xyzw)
};

struct AnimationClip
{
    std::string                   name;
    float                         duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

// ---------------------------------------------------------------------------
// Interpolation helpers (inline — used by AnimationController and unit tests)
// ---------------------------------------------------------------------------

inline DirectX::XMFLOAT3 InterpolateFloat3(
    const std::vector<Keyframe<DirectX::XMFLOAT3>>& keys,
    float time,
    AnimationInterpolation interp)
{
    if (keys.empty())
        return { 0.0f, 0.0f, 0.0f };
    if (keys.size() == 1 || time <= keys.front().time)
        return keys.front().value;
    if (time >= keys.back().time)
        return keys.back().value;

    // Binary search for surrounding pair
    size_t hi = 1;
    while (hi < keys.size() && keys[hi].time < time)
        ++hi;
    const size_t lo = hi - 1;

    if (interp == AnimationInterpolation::Step)
        return keys[lo].value;

    const float alpha = (time - keys[lo].time) / (keys[hi].time - keys[lo].time);
    DirectX::XMFLOAT3 result;
    DirectX::XMStoreFloat3(&result,
        DirectX::XMVectorLerp(
            DirectX::XMLoadFloat3(&keys[lo].value),
            DirectX::XMLoadFloat3(&keys[hi].value),
            alpha));
    return result;
}

inline DirectX::XMFLOAT4 InterpolateQuaternion(
    const std::vector<Keyframe<DirectX::XMFLOAT4>>& keys,
    float time,
    AnimationInterpolation interp)
{
    const DirectX::XMFLOAT4 identity = { 0.0f, 0.0f, 0.0f, 1.0f };
    if (keys.empty())
        return identity;
    if (keys.size() == 1 || time <= keys.front().time)
        return keys.front().value;
    if (time >= keys.back().time)
        return keys.back().value;

    size_t hi = 1;
    while (hi < keys.size() && keys[hi].time < time)
        ++hi;
    const size_t lo = hi - 1;

    if (interp == AnimationInterpolation::Step)
        return keys[lo].value;

    const float alpha = (time - keys[lo].time) / (keys[hi].time - keys[lo].time);
    DirectX::XMFLOAT4 result;
    DirectX::XMStoreFloat4(&result,
        DirectX::XMQuaternionSlerp(
            DirectX::XMLoadFloat4(&keys[lo].value),
            DirectX::XMLoadFloat4(&keys[hi].value),
            alpha));
    return result;
}

} // namespace RRE
