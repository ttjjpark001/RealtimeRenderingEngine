#pragma once

#include <DirectXMath.h>
#include "Core/Types.h"
#include <vector>
#include <set>

namespace RRE
{

class FaceColorPalette
{
public:
    static constexpr uint32 PALETTE_SIZE = 8;

    static const DirectX::XMFLOAT4& GetColor(uint32 paletteIndex)
    {
        static const DirectX::XMFLOAT4 palette[PALETTE_SIZE] =
        {
            { 1.0f, 0.0f, 0.0f, 1.0f },  // Red
            { 0.0f, 1.0f, 0.0f, 1.0f },  // Green
            { 0.0f, 0.0f, 1.0f, 1.0f },  // Blue
            { 0.0f, 1.0f, 1.0f, 1.0f },  // Cyan
            { 1.0f, 0.0f, 1.0f, 1.0f },  // Magenta
            { 1.0f, 1.0f, 0.0f, 1.0f },  // Yellow
            { 0.0f, 0.0f, 0.0f, 1.0f },  // Black
            { 1.0f, 1.0f, 1.0f, 1.0f },  // White
        };
        return palette[paletteIndex % PALETTE_SIZE];
    }

    // Greedy graph coloring: assign colors so adjacent faces have different colors
    static std::vector<uint32> AssignFaceColors(const std::vector<std::vector<uint32>>& adjacency)
    {
        uint32 faceCount = static_cast<uint32>(adjacency.size());
        std::vector<uint32> colors(faceCount, UINT32_MAX);

        for (uint32 face = 0; face < faceCount; ++face)
        {
            // Collect colors used by neighbors
            std::set<uint32> usedColors;
            for (uint32 neighbor : adjacency[face])
            {
                if (colors[neighbor] != UINT32_MAX)
                {
                    usedColors.insert(colors[neighbor]);
                }
            }

            // Pick the lowest available color
            for (uint32 c = 0; c < PALETTE_SIZE; ++c)
            {
                if (usedColors.find(c) == usedColors.end())
                {
                    colors[face] = c;
                    break;
                }
            }
        }

        return colors;
    }
};

} // namespace RRE
