#pragma once

#include "Core/Types.h"

namespace RRE
{

class IRHIBuffer
{
public:
    virtual ~IRHIBuffer() = default;

    virtual void SetData(const void* data, uint32 size, uint32 stride) = 0;
    virtual uint32 GetSize() const = 0;
    virtual uint32 GetStride() const = 0;
};

} // namespace RRE
