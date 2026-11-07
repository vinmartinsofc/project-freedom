#pragma once
#include <juce_graphics/juce_graphics.h>

// Paleta própria do Project Freedom (escura, com acento verde-água).
// Fica num lugar só para o tema virar configurável depois.
namespace theme
{
// Texto da interface em UTF-8 (acentos corretos em qualquer compilador).
inline juce::String ui (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::Colour background()   { return juce::Colour (0xff1b1d21); }
inline juce::Colour panel()        { return juce::Colour (0xff24272c); }
inline juce::Colour panelLight()   { return juce::Colour (0xff2e3238); }
inline juce::Colour border()       { return juce::Colour (0xff3a3f46); }
inline juce::Colour text()         { return juce::Colour (0xffd8dce1); }
inline juce::Colour textDim()      { return juce::Colour (0xff8b929b); }
inline juce::Colour accent()       { return juce::Colour (0xff3fd0c0); }
inline juce::Colour noteFill()     { return juce::Colour (0xff3fd0c0); }
inline juce::Colour noteSelected() { return juce::Colour (0xffffd166); }
inline juce::Colour rowWhite()     { return juce::Colour (0xff2c3036); }
inline juce::Colour rowBlack()     { return juce::Colour (0xff23262b); }
inline juce::Colour rowInScale()   { return juce::Colour (0xff35505a); }
inline juce::Colour playhead()     { return juce::Colour (0xffffffff); }
} // namespace theme
