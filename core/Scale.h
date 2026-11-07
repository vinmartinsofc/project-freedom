#pragma once
#include <string_view>
#include <vector>

namespace roll
{
enum class Mode
{
    Major, NaturalMinor, Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    HarmonicMinor, MelodicMinor
};

// Campo harmônico/escala: tônica (0=C ... 11=B) + modo.
class Scale
{
public:
    Scale() = default;
    Scale(int rootPitchClass, Mode mode);

    int  root() const { return rootPc; }
    Mode mode() const { return m; }

    // A nota MIDI (qualquer oitava) pertence à escala?
    bool contains(int midiNote) const;

    // Graus da escala como pitch classes (0..11), a partir da tônica.
    std::vector<int> pitchClasses() const;

    static std::vector<int> intervalsOf(Mode mode);
    static std::string_view nameOf(Mode mode);

private:
    int  rootPc = 0;
    Mode m = Mode::Major;
};
} // namespace roll
