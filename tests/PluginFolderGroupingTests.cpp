#include <catch2/catch_test_macros.hpp>
#include "PluginFolderGrouping.h"

using namespace roll;

TEST_CASE ("Plugin folders follow the first directory below the VST3 root")
{
    const std::vector<PluginPath> plugins
    {
        { "Pro-Q", R"(C:\Program Files\Common Files\VST3\FabFilter\Pro-Q.vst3)" },
        { "RX", R"(C:\Program Files\Common Files\VST3\iZotope, Inc.\RX.vst3)" },
        { "Nexus", R"(C:\Program Files\Common Files\VST3\Nexus.vst3)" },
        { "Pro-C", R"(C:\Program Files\Common Files\VST3\FabFilter\Pro-C.vst3)" }
    };

    const auto groups = groupPluginsByFolder (plugins, R"(C:\Program Files\Common Files\VST3)");

    REQUIRE (groups.size() == 3);
    CHECK (groups[0].folder.empty());
    CHECK (groups[0].plugins[0].name == "Nexus");
    CHECK (groups[1].folder == "FabFilter");
    REQUIRE (groups[1].plugins.size() == 2);
    CHECK (groups[1].plugins[0].name == "Pro-C");
    CHECK (groups[1].plugins[1].name == "Pro-Q");
    CHECK (groups[2].folder == "iZotope, Inc.");
}

TEST_CASE ("Plugin folder matching is case-insensitive and preserves folder spelling")
{
    const std::vector<PluginPath> plugins
    {
        { "Plugin", R"(c:\program files\common files\vst3\Vendor Name\Plugin.vst3)" }
    };

    const auto groups = groupPluginsByFolder (plugins, R"(C:\Program Files\Common Files\VST3)");

    REQUIRE (groups.size() == 1);
    CHECK (groups[0].folder == "Vendor Name");
}
