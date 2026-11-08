#pragma once

#include <vector>

namespace roll
{

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    constexpr int right() const noexcept { return x + width; }
    constexpr int bottom() const noexcept { return y + height; }
    constexpr bool operator== (const Rect&) const noexcept = default;
};

class WindowPlacement
{
public:
    static Rect defaultMainWindowBounds (const Rect& displayUserArea,
                                         int minWidth = 1000,
                                         int minHeight = 620,
                                         float screenFraction = 0.85f);

    static Rect clampToActiveDisplays (const Rect& targetBounds,
                                       const std::vector<Rect>& displayUserAreas,
                                       const Rect& fallbackUserArea,
                                       int minWidth = 1000,
                                       int minHeight = 620);

    static Rect placePluginWindow (const Rect& pluginBounds,
                                   const Rect& displayUserArea,
                                   int cascadeIndex = 0,
                                   int cascadeStep = 28);
};

} // namespace roll
