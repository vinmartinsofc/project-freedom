#pragma once
#include "Note.h"
#include <cstdint>
#include <vector>

namespace roll
{
struct MidiEvent
{
    int  offset   = 0;      // amostras desde o início do bloco
    bool on       = true;
    int  pitch    = 60;
    int  velocity = 100;
};

struct TransportSettings
{
    double bpm       = 120.0;
    bool   loop      = true;
    double loopStart = 0.0;     // em beats
    double loopEnd   = 16.0;
};

// Um trecho contínuo de tempo dentro de um bloco de áudio (o loop pode dividir o bloco em dois).
struct Segment
{
    double a = 0.0, b = 0.0;      // beats [a, b)
    int    offset = 0;            // amostra em que o trecho começa no bloco
    int    samples = 0;
    bool   flushBefore = false;   // soltar notas antes do trecho (volta do loop)
    bool   flushAfter  = false;   // soltar notas ao fim do trecho (fim do loop)
};

struct BlockPlan
{
    static constexpr int maxSegments = 8;
    Segment seg[maxSegments];
    int    count = 0;
    int    numSamples = 0;
    double beatsPerSample = 0.0;
};

// Relógio do transporte: UM por projeto. Decide que trecho de tempo cada bloco cobre.
class TransportClock
{
public:
    double position() const        { return pos; }
    void   setPosition (double b)  { pos = b; }
    void   plan (const TransportSettings& t, double sampleRate, int numSamples, BlockPlan& out);

private:
    double pos = 0.0;
};

// Gerador de eventos MIDI: UM por trilha. Lógica pura, sem JUCE e sem alocação no caminho normal.
class Sequencer
{
public:
    Sequencer() { active.reserve (1024); }

    void process (const std::vector<Note>& notes, const BlockPlan& plan, std::vector<MidiEvent>& out);
    void flush (int sampleOffset, std::vector<MidiEvent>& out);

    bool hasActiveNotes() const { return !active.empty(); }
    void activeMask (std::uint64_t& lo, std::uint64_t& hi) const;   // pitches soando (bits 0..127)

private:
    struct Active { int id; int pitch; double end; };

    void emit (const std::vector<Note>& notes, double a, double b,
               int baseOffset, double beatsPerSample, std::vector<MidiEvent>& out);

    std::vector<Active> active;
};
} // namespace roll
