#pragma once

#include "Renderer/Mesh.h"
#include "Core/Types.h"

namespace RRE
{

class MeshFactory
{
public:
    static Mesh CreateSphere(uint32 segments = 16, uint32 rings = 16);
    static Mesh CreateTetrahedron();
    static Mesh CreateCube();
    static Mesh CreateCylinder(uint32 segments = 16, float height = 2.0f);
};

} // namespace RRE
