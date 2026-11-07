#include "Scale.h"

namespace roll
{
Scale::Scale(int rootPitchClass, Mode mode)
    : rootPc(((rootPitchClass % 12) + 12) % 12), m(mode) {}

std::vector<int> Scale::intervalsOf(Mode mode)
{
    switch (mode)
    {
        case Mode::Major:         return { 0, 2, 4, 5, 7, 9, 11 };
        case Mode::NaturalMinor:  return { 0, 2, 3, 5, 7, 8, 10 };
        case Mode::Dorian:        return { 0, 2, 3, 5, 7, 9, 10 };
        case Mode::Phrygian:      return { 0, 1, 3, 5, 7, 8, 10 };
        case Mode::Lydian:        return { 0, 2, 4, 6, 7, 9, 11 };
        case Mode::Mixolydian:    return { 0, 2, 4, 5, 7, 9, 10 };
        case Mode::Locrian:       return { 0, 1, 3, 5, 6, 8, 10 };
        case Mode::HarmonicMinor: return { 0, 2, 3, 5, 7, 8, 11 };
        case Mode::MelodicMinor:  return { 0, 2, 3, 5, 7, 9, 11 };
    }
    return {};
}

std::string_view Scale::nameOf(Mode mode)
{
    switch (mode)
    {
        case Mode::Major:         return "Major";
        case Mode::NaturalMinor:  return "Minor";
        case Mode::Dorian:        return "Dorian";
        case Mode::Phrygian:      return "Phrygian";
        case Mode::Lydian:        return "Lydian";
        case Mode::Mixolydian:    return "Mixolydian";
        case Mode::Locrian:       return "Locrian";
        case Mode::HarmonicMinor: return "Harmonic Minor";
        case Mode::MelodicMinor:  return "Melodic Minor";
    }
    return "";
}

std::vector<int> Scale::pitchClasses() const
{
    std::vector<int> out;
    for (int i : intervalsOf(m))
        out.push_back((rootPc + i) % 12);
    return out;
}

bool Scale::contains(int midiNote) const
{
    const int pc = ((midiNote % 12) + 12) % 12;
    for (int i : intervalsOf(m))
        if ((rootPc + i) % 12 == pc)
            return true;
    return false;
}
} // namespace roll
