#pragma once
#include <juce_graphics/juce_graphics.h>

// Paleta grafite do Project Freedom.
namespace theme
{
// Texto da interface em UTF-8 (acentos corretos em qualquer compilador).
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::Colour background()   { return juce::Colour (0xff343638); }
inline juce::Colour panel()        { return juce::Colour (0xff292b2d); }
inline juce::Colour panelLight()   { return juce::Colour (0xff303234); }
inline juce::Colour control()      { return juce::Colour (0xff242628); }
inline juce::Colour hover()        { return juce::Colour (0xff2c2e31); }
inline juce::Colour pressed()      { return juce::Colour (0xff1d1f21); }
inline juce::Colour border()       { return juce::Colour (0xff45484a); }
inline juce::Colour text()         { return juce::Colour (0xffe6e6e6); }
inline juce::Colour textDim()      { return juce::Colour (0xffa5a7a8); }
inline juce::Colour accent()       { return juce::Colour (0xff66a3ff); }
inline juce::Colour recording()    { return juce::Colour (0xffe85b5b); }
inline juce::Colour warning()      { return juce::Colour (0xfff2bd50); }
inline juce::Colour noteFill()     { return accent(); }
inline juce::Colour noteSelected() { return warning(); }
inline juce::Colour rowWhite()     { return juce::Colour (0xff292b2d); }
inline juce::Colour rowBlack()     { return juce::Colour (0xff242628); }
inline juce::Colour rowInScale()   { return juce::Colour (0xff30445e); }
inline juce::Colour playhead()     { return juce::Colour (0xffffffff); }
} // namespace theme
