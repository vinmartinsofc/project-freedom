#pragma once

namespace roll
{
// Uma nota MIDI dentro de um clip. Tempo medido em beats (semínimas),
// independente de BPM e de sample rate.
struct Note
{
    int    pitch       = 60;    // 0..127
    double startBeats  = 0.0;
    double lengthBeats = 1.0;
    int    velocity    = 100;   // 1..127
    bool   muted       = false; // nota desativada (nao toca)
    int    id          = 0;     // atribuido pelo Clip, estavel durante a vida da nota

    double endBeats() const { return startBeats + lengthBeats; }
};
} // namespace roll
