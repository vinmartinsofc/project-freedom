#include <catch2/catch_test_macros.hpp>
#include "WindowPlacement.h"

using namespace roll;

TEST_CASE ("WindowPlacement defaults the main window within the usable display area")
{
    SECTION ("standard desktop")
    {
        const Rect display { 0, 0, 1920, 1040 };
        const auto bounds = WindowPlacement::defaultMainWindowBounds (display);

        CHECK (bounds == (Rect { 144, 78, 1632, 884 }));
    }

    SECTION ("screen smaller than the preferred minimum")
    {
        const Rect display { 0, 0, 800, 560 };
        const auto bounds = WindowPlacement::defaultMainWindowBounds (display);

        CHECK (bounds.width == 800);
        CHECK (bounds.height == 560);
        CHECK (bounds.x == display.x);
        CHECK (bounds.y == display.y);
    }
}

TEST_CASE ("WindowPlacement restores saved bounds on an active display")
{
    const Rect primary { 0, 0, 1920, 1040 };
    const Rect secondary { 1920, 0, 1366, 728 };
    const std::vector<Rect> displays { primary, secondary };

    SECTION ("off-screen bounds return to the primary display")
    {
        const auto bounds = WindowPlacement::clampToActiveDisplays (
            { 5000, 4000, 1200, 700 }, displays, primary);

        CHECK (bounds.x >= primary.x);
        CHECK (bounds.y >= primary.y);
        CHECK (bounds.right() <= primary.right());
        CHECK (bounds.bottom() <= primary.bottom());
    }

    SECTION ("partially clipped bounds are shifted inside their display")
    {
        const auto bounds = WindowPlacement::clampToActiveDisplays (
            { 3000, 500, 1200, 700 }, displays, primary);

        CHECK (bounds.x >= secondary.x);
        CHECK (bounds.y >= secondary.y);
        CHECK (bounds.right() <= secondary.right());
        CHECK (bounds.bottom() <= secondary.bottom());
    }

    SECTION ("valid bounds on a secondary display are retained")
    {
        const auto bounds = WindowPlacement::clampToActiveDisplays (
            { 2050, 40, 1000, 640 }, displays, primary);

        CHECK (bounds == (Rect { 2050, 40, 1000, 640 }));
    }
}

TEST_CASE ("WindowPlacement cascades plugin windows and keeps them visible")
{
    const Rect display { 100, 50, 1600, 900 };
    const auto first = WindowPlacement::placePluginWindow ({ 0, 0, 600, 400 }, display);
    const auto second = WindowPlacement::placePluginWindow ({ 0, 0, 600, 400 }, display, 1);

    CHECK (second.x == first.x + 28);
    CHECK (second.y == first.y + 28);

    const auto oversized = WindowPlacement::placePluginWindow ({ 0, 0, 2400, 1200 }, display);
    CHECK (oversized.width == display.width);
    CHECK (oversized.height == display.height);
    CHECK (oversized.x == display.x);
    CHECK (oversized.y == display.y);
}
