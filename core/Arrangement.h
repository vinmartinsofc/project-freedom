#pragma once
#include "Clip.h"
#include <memory>
#include <string>
#include <vector>

namespace roll
{
enum class TrackType { Midi, Audio };
enum class TrackOutputRouting { Master, ExtOut };

// Um clip colocado na linha do tempo de uma trilha (posição em beats).
struct Placement
{
    int         id = 0;
    double      start = 0.0;
    std::string name;
    Clip        clip { 4.0 };

    double end() const { return start + clip.length(); }
};

struct AudioPlacement
{
    int         id = 0;
    double      start = 0.0;
    double      lengthBeats = 0.0;
    double      durationSeconds = 0.0;
    double      sourceBpm = 120.0;
    std::string name, filePath;
    std::vector<float> waveform;

    double end() const { return start + lengthBeats; }
};

// Trilha + parâmetros de mixagem. Clips ficam em heap (ponteiros estáveis).
class Track
{
public:
    int         id = 0;
    std::string name;
    TrackType   type = TrackType::Midi;
    double      volume = 1.0;     // ganho linear (0..1.5)
    double      pan = 0.0;        // -1..1
    bool        mute = false;
    bool        solo = false;
    TrackOutputRouting outputRouting = TrackOutputRouting::Master;

    std::vector<std::unique_ptr<Placement>> clips;
    std::vector<std::unique_ptr<AudioPlacement>> audioClips;

    Placement* addClip (double start, double length);
    Placement* find (int clipId) const;
    std::unique_ptr<Placement> take (int clipId);          // tira da trilha (para mover entre trilhas)
    void put (std::unique_ptr<Placement> p);
    bool remove (int clipId);

    // O trecho [start, start+length) cabe sem encostar em outro clip (ignorando 'ignoreId')?
    bool isFree (double start, double length, int ignoreId = -1) const;

    AudioPlacement* addAudioClip (double start, double length, std::string filePath,
                                  std::string name, std::vector<float> waveform = {},
                                  double durationSeconds = 0.0, double sourceBpm = 120.0);
    AudioPlacement* findAudioClip (int clipId) const;
    std::unique_ptr<AudioPlacement> takeAudioClip (int clipId);
    void putAudioClip (std::unique_ptr<AudioPlacement> clip);
    bool removeAudioClip (int clipId);
    bool isAudioFree (double start, double length, int ignoreId = -1) const;

    // Id de clip único dentro da trilha.
    int nextClipId() { return nextId++; }

private:
    int nextId = 1;
};

// Junta os clips da trilha numa lista de notas em tempo absoluto (o que o sequenciador toca).
// Notas além do fim do clip são cortadas/descartadas.
std::vector<Note> flatten (const Track& track);

// Conversao linear <-> decibeis para faders e medidores
float gainToDecibels (float gain, float minusInfinityDb = -60.0f);
float decibelsToGain (float decibels);
} // namespace roll
