#pragma once
#include "Clip.h"
#include <memory>
#include <string>
#include <vector>

namespace roll
{
// Um clip colocado na linha do tempo de uma trilha (posição em beats).
struct Placement
{
    int         id = 0;
    double      start = 0.0;
    std::string name;
    Clip        clip { 4.0 };

    double end() const { return start + clip.length(); }
};

// Trilha MIDI: clips + parâmetros de mixagem. Os clips ficam em heap (ponteiros estáveis).
class Track
{
public:
    int         id = 0;
    std::string name;
    double      volume = 0.8;     // ganho linear (0..1.5)
    double      pan = 0.0;        // -1..1
    bool        mute = false;
    bool        solo = false;

    std::vector<std::unique_ptr<Placement>> clips;

    Placement* addClip (double start, double length);
    Placement* find (int clipId) const;
    std::unique_ptr<Placement> take (int clipId);          // tira da trilha (para mover entre trilhas)
    void put (std::unique_ptr<Placement> p);
    bool remove (int clipId);

    // O trecho [start, start+length) cabe sem encostar em outro clip (ignorando 'ignoreId')?
    bool isFree (double start, double length, int ignoreId = -1) const;

    // Id de clip único dentro da trilha.
    int nextClipId() { return nextId++; }

private:
    int nextId = 1;
};

// Junta os clips da trilha numa lista de notas em tempo absoluto (o que o sequenciador toca).
// Notas além do fim do clip são cortadas/descartadas.
std::vector<Note> flatten (const Track& track);
} // namespace roll
