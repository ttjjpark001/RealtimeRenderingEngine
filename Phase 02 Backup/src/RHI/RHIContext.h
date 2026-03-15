#pragma once

#include "Core/Types.h"
#include <DirectXMath.h>

namespace RRE
{

class IRHIBuffer;

class IRHIContext
{
public:
    virtual ~IRHIContext() = default;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Clear(const DirectX::XMFLOAT4& color) = 0;
    virtual void DrawPrimitives(IRHIBuffer* vb, IRHIBuffer* ib,
        const DirectX::XMFLOAT4X4& worldMatrix) = 0;
    virtual void DrawText(int x, int y, const char* text,
        const DirectX::XMFLOAT4& color) = 0;
};

} // namespace RRE
