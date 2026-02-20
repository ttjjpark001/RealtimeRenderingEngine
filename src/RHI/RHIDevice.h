#pragma once

#include "Core/Types.h"

namespace RRE
{

class IRHIContext;

class IRHIDevice
{
public:
    virtual ~IRHIDevice() = default;

    virtual bool Initialize(void* windowHandle, uint32 width, uint32 height) = 0;
    virtual void Shutdown() = 0;
    virtual void OnResize(uint32 width, uint32 height) = 0;
    virtual IRHIContext* GetContext() = 0;
};

} // namespace RRE
