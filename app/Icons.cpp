#include "Icons.h"
#include <BinaryData.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>

namespace icons
{
namespace
{
constexpr std::array<const char*, 24> resourceNames
{
    "play_svg", "pause_svg", "stop_svg", "record_svg", "rewind_svg", "fastforward_svg",
    "loop_svg", "metronome_svg", "pointer_svg", "pencil_svg", "eraser_svg", "cut_svg",
    "copy_svg", "move_svg", "selectregion_svg", "split_svg", "mixer_svg", "volume_svg",
    "mute_svg", "pan_svg", "levelmeter_svg", "automation_svg", "keyboard_svg", "waveform_svg"
};

using CacheKey = std::tuple<Icon, std::uint32_t, int>;

std::map<CacheKey, std::unique_ptr<juce::Drawable>>& cache()
{
    static std::map<CacheKey, std::unique_ptr<juce::Drawable>> drawables;
    return drawables;
}

void replaceAll (std::string& text, const std::string& from, const std::string& to)
{
    std::size_t position = 0;
    while ((position = text.find (from, position)) != std::string::npos)
    {
        text.replace (position, from.size(), to);
        position += to.size();
    }
}
}

const juce::Drawable* get (Icon icon, juce::Colour colour, int size)
{
    const auto index = static_cast<std::size_t> (icon);
    if (index >= resourceNames.size())
        return nullptr;

    size = std::max (1, size);
    const CacheKey key { icon, colour.getARGB(), size };
    auto& drawables = cache();
    if (const auto found = drawables.find (key); found != drawables.end())
        return found->second.get();

    int dataSize = 0;
    const auto* data = BinaryData::getNamedResource (resourceNames[index], dataSize);
    if (data == nullptr || dataSize <= 0)
        return nullptr;

    std::string svg (data, static_cast<std::size_t> (dataSize));
    replaceAll (svg, "currentColor", colour.toDisplayString (false).toStdString());
    auto drawable = juce::Drawable::createFromImageData (svg.data(), svg.size());
    if (drawable == nullptr)
        return nullptr;

    const auto* result = drawable.get();
    drawables.emplace (key, std::move (drawable));
    return result;
}
}
