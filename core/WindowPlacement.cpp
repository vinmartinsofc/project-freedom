#include "WindowPlacement.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace roll
{
namespace
{
int clampDimension (int value, int minimum, int maximum)
{
    const int safeMaximum = std::max (1, maximum);
    const int safeMinimum = std::clamp (minimum, 1, safeMaximum);
    return std::clamp (value, safeMinimum, safeMaximum);
}

std::int64_t intersectionArea (const Rect& a, const Rect& b)
{
    const auto width = std::max (0, std::min (a.right(), b.right()) - std::max (a.x, b.x));
    const auto height = std::max (0, std::min (a.bottom(), b.bottom()) - std::max (a.y, b.y));
    return static_cast<std::int64_t> (width) * height;
}
}

Rect WindowPlacement::defaultMainWindowBounds (const Rect& displayUserArea,
                                               int minWidth,
                                               int minHeight,
                                               float screenFraction)
{
    const int areaWidth = std::max (1, displayUserArea.width);
    const int areaHeight = std::max (1, displayUserArea.height);
    const float fraction = std::clamp (screenFraction, 0.0f, 1.0f);
    const int width = clampDimension (static_cast<int> (std::lround (areaWidth * fraction)),
                                     minWidth, areaWidth);
    const int height = clampDimension (static_cast<int> (std::lround (areaHeight * fraction)),
                                      minHeight, areaHeight);

    return { displayUserArea.x + (displayUserArea.width - width) / 2,
             displayUserArea.y + (displayUserArea.height - height) / 2,
             width, height };
}

Rect WindowPlacement::clampToActiveDisplays (const Rect& targetBounds,
                                             const std::vector<Rect>& displayUserAreas,
                                             const Rect& fallbackUserArea,
                                             int minWidth,
                                             int minHeight)
{
    const Rect* bestDisplay = nullptr;
    std::int64_t largestIntersection = 0;

    for (const auto& display : displayUserAreas)
    {
        const auto area = intersectionArea (targetBounds, display);
        if (area > largestIntersection)
        {
            largestIntersection = area;
            bestDisplay = &display;
        }
    }

    if (bestDisplay == nullptr)
        return defaultMainWindowBounds (fallbackUserArea, minWidth, minHeight);

    const auto& display = *bestDisplay;
    const int width = clampDimension (targetBounds.width, minWidth, display.width);
    const int height = clampDimension (targetBounds.height, minHeight, display.height);
    const int x = std::clamp (targetBounds.x, display.x, display.right() - width);
    const int y = std::clamp (targetBounds.y, display.y, display.bottom() - height);

    return { x, y, width, height };
}

Rect WindowPlacement::placePluginWindow (const Rect& pluginBounds,
                                         const Rect& displayUserArea,
                                         int cascadeIndex,
                                         int cascadeStep)
{
    const int width = clampDimension (pluginBounds.width, 1, displayUserArea.width);
    const int height = clampDimension (pluginBounds.height, 1, displayUserArea.height);
    constexpr int baseMargin = 60;
    constexpr int cascadeSlots = 8;
    const int slot = ((cascadeIndex % cascadeSlots) + cascadeSlots) % cascadeSlots;
    const int offset = slot * std::max (0, cascadeStep);
    const int x = std::clamp (displayUserArea.x + baseMargin + offset,
                              displayUserArea.x, displayUserArea.right() - width);
    const int y = std::clamp (displayUserArea.y + baseMargin + offset,
                              displayUserArea.y, displayUserArea.bottom() - height);

    return { x, y, width, height };
}

} // namespace roll
