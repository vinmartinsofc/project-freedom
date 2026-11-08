#pragma once

#include <string>
#include <vector>

namespace roll
{
struct PluginPath
{
    std::string name;
    std::string path;
};

struct PluginFolderGroup
{
    std::string folder;
    std::vector<PluginPath> plugins;
};

std::vector<PluginFolderGroup> groupPluginsByFolder (const std::vector<PluginPath>& plugins,
                                                      const std::string& rootPath);
}
