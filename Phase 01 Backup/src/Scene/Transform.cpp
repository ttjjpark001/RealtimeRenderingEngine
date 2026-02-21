#include "Scene/Transform.h"
#include "Math/MathUtil.h"

namespace RRE
{

DirectX::XMMATRIX Transform::GetLocalMatrix() const
{
    return Math::CreateTRSMatrix(m_position, m_rotation, m_scale);
}

} // namespace RRE
