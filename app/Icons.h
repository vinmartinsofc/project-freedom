#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace icons
{
enum class Icon
{
    Play,
    Pause,
    Stop,
    Record,
    Rewind,
    FastForward,
    Loop,
    Metronome,
    Pointer,
    Pencil,
    Eraser,
    Cut,
    Copy,
    Move,
    SelectRegion,
    Split,
    Mixer,
    Volume,
    Mute,
    Pan,
    LevelMeter,
    Automation,
    Keyboard,
    Waveform
};

const juce::Drawable* get (Icon icon, juce::Colour colour, int size = 24);
}
