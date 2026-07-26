#pragma once

#include "Core/Globals/_TextureIndex.h"

namespace CharacterSceneTextures
{
    constexpr int Begin = 31970;
    constexpr int SelectRowNormal = Begin;
    constexpr int SelectRowActive = Begin + 1;
    constexpr int SelectArrowUp = Begin + 2;
    constexpr int SelectArrowDown = Begin + 3;
    constexpr int CreateDecoration = Begin + 4;
    constexpr int CreateClassButton = Begin + 5;
    constexpr int CreateActionButton = Begin + 6;
    constexpr int CreateInput = Begin + 7;
    constexpr int SelectRowHover = Begin + 8;
    constexpr int SelectRowSelected = Begin + 9;
    constexpr int CreateClassBeam = Begin + 10;
    constexpr int SelectPowerIcon = Begin + 11;
    constexpr int SelectPowerDigits = Begin + 12;
    constexpr int End = Begin + 13;

    static_assert(Begin > BITMAP_FONT_HIT + 4);
    static_assert(End < BITMAP_INTERFACE_TEXTURE_END);
}
