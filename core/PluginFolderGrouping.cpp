#include "PluginFolderGrouping.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <utility>

namespace roll
{
namespace
{
std::string normalisePath (std::string path)
{
    std::replace (path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();
    return path;
}

char lowerAscii (char c)
{
    return static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
}

bool startsWithIgnoringCase (const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size()
        && std::equal (prefix.begin(), prefix.end(), value.begin(),
                       [] (char a, char b) { return lowerAscii (a) == lowerAscii (b); });
}

std::string firstFolderBelowRoot (const std::string& pluginPath, const std::string& rootPath)
{
    const auto path = normalisePath (pluginPath);
    const auto root = normalisePath (rootPath);
    if (root.empty() || ! startsWithIgnoringCase (path, root)
        || path.size() <= root.size() || path[root.size()] != '/')
        return {};

    const auto relative = path.substr (root.size() + 1);
    const auto separator = relative.find ('/');
    if (separator == std::string::npos)
        return {};

    auto folder = relative.substr (0, separator);
    const auto extension = folder.find_last_of ('.');
    if (extension != std::string::npos
        && lowerAscii (folder[extension + 1]) == 'v')
    {
        std::string lowerExtension;
        lowerExtension.reserve (folder.size() - extension);
        std::transform (folder.begin() + static_cast<std::ptrdiff_t> (extension), folder.end(),
                        std::back_inserter (lowerExtension), lowerAscii);
        if (lowerExtension == ".vst3")
            return {};
    }

    return folder;
}
}

std::vector<PluginFolderGroup> groupPluginsByFolder (const std::vector<PluginPath>& plugins,
                                                      const std::string& rootPath)
{
    std::vector<PluginFolderGroup> groups;
    for (const auto& plugin : plugins)
    {
        const auto folder = firstFolderBelowRoot (plugin.path, rootPath);
        auto it = std::find_if (groups.begin(), groups.end(), [&folder] (const auto& group)
        {
            return group.folder.size() == folder.size()
                && std::equal (group.folder.begin(), group.folder.end(), folder.begin(),
                               [] (char a, char b) { return lowerAscii (a) == lowerAscii (b); });
        });

        if (it == groups.end())
        {
            groups.push_back ({ folder, {} });
            it = std::prev (groups.end());
        }
        it->plugins.push_back (plugin);
    }

    const auto compareIgnoreCase = [] (const auto& a, const auto& b)
    {
        return std::lexicographical_compare (a.begin(), a.end(), b.begin(), b.end(),
                                             [] (char x, char y) { return lowerAscii (x) < lowerAscii (y); });
    };

    for (auto& group : groups)
        std::sort (group.plugins.begin(), group.plugins.end(), [&compareIgnoreCase] (const auto& a, const auto& b)
        {
            return compareIgnoreCase (a.name, b.name);
        });

    std::sort (groups.begin(), groups.end(), [&compareIgnoreCase] (const auto& a, const auto& b)
    {
        if (a.folder.empty() != b.folder.empty())
            return a.folder.empty();
        return compareIgnoreCase (a.folder, b.folder);
    });

    return groups;
}
}
