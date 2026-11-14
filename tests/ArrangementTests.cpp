#include <catch2/catch_test_macros.hpp>
#include "Arrangement.h"
#include <cmath>

using namespace roll;

TEST_CASE("Clips nao podem se sobrepor na mesma trilha")
{
    Track t;
    auto* a = t.addClip(0.0, 4.0);
    REQUIRE(t.isFree(4.0, 4.0));              // encostar e permitido
    REQUIRE_FALSE(t.isFree(3.0, 2.0));
    REQUIRE(t.isFree(0.0, 4.0, a->id));       // ignora a si mesmo
}

TEST_CASE("flatten converte notas do clip para tempo absoluto")
{
    Track t;
    auto* c = t.addClip(8.0, 4.0);             // comeca no beat 8
    c->clip.addNote({ 60, 1.0, 1.0, 100 });
    auto flat = flatten(t);
    REQUIRE(flat.size() == 1);
    REQUIRE(flat[0].startBeats == 9.0);
}

TEST_CASE("flatten corta notas que passam do fim do clip e descarta as de fora")
{
    Track t;
    auto* c = t.addClip(0.0, 4.0);
    c->clip.addNote({ 60, 3.5, 2.0, 100 });    // sai do clip: cortada em 0.5
    c->clip.addNote({ 62, 5.0, 1.0, 100 });    // totalmente fora
    auto flat = flatten(t);
    REQUIRE(flat.size() == 1);
    REQUIRE(flat[0].lengthBeats == 0.5);
}

TEST_CASE("Mover clip entre trilhas preserva as notas")
{
    Track a, b;
    auto* c = a.addClip(0.0, 4.0);
    c->clip.addNote({ 60, 0.0, 1.0, 100 });
    int id = c->id;
    auto taken = a.take(id);
    REQUIRE(a.clips.empty());
    b.put(std::move(taken));
    REQUIRE(b.clips.size() == 1);
    REQUIRE(b.clips[0]->clip.notes().size() == 1);
}

TEST_CASE("Copiar clip MIDI preserva notas e nao altera o original")
{
    Track track;
    auto* original = track.addClip (0.0, 4.0);
    original->name = "riff";
    original->clip.addNote ({ 60, 1.0, 2.0, 90 });

    auto* copy = track.copyClip (*original, 4.0);
    REQUIRE(copy != nullptr);
    REQUIRE(copy->start == 4.0);
    REQUIRE(copy->name == "riff");
    REQUIRE(copy->clip.notes().size() == 1);
    REQUIRE(copy->clip.notes()[0].pitch == 60);
    REQUIRE(original->start == 0.0);
    REQUIRE(track.copyClip (*original, 2.0) == nullptr);
}

TEST_CASE("Dividir clip MIDI preserva notas e divide notas que cruzam o corte")
{
    Track track;
    auto* original = track.addClip (4.0, 8.0);
    original->name = "frase";
    original->clip.addNote ({ 60, 0.0, 2.0, 90 });
    original->clip.addNote ({ 62, 3.0, 4.0, 80 });
    original->clip.addNote ({ 64, 6.0, 1.0, 70 });

    const int rightId = track.splitClip (original->id, 8.0);
    REQUIRE(rightId > 0);
    REQUIRE(original->start == 4.0);
    REQUIRE(original->end() == 8.0);
    REQUIRE(original->clip.notes().size() == 2);
    REQUIRE(original->clip.notes()[1].lengthBeats == 1.0);

    const auto* right = track.find (rightId);
    REQUIRE(right != nullptr);
    REQUIRE(right->name == "frase");
    REQUIRE(right->start == 8.0);
    REQUIRE(right->end() == 12.0);
    REQUIRE(right->clip.notes().size() == 2);
    REQUIRE(right->clip.notes()[0].startBeats == 0.0);
    REQUIRE(right->clip.notes()[0].lengthBeats == 3.0);
    REQUIRE(right->clip.notes()[1].startBeats == 2.0);
}

TEST_CASE("Clips de audio preservam arquivo, waveform e limites de sobreposicao")
{
    Track track;
    track.type = TrackType::Audio;
    auto* a = track.addAudioClip (2.0, 3.0, "C:/audio/kick.wav", "kick.wav", { 0.2f, 0.8f }, 1.5, 120.0);

    REQUIRE(a->end() == 5.0);
    REQUIRE(a->durationSeconds == 1.5);
    REQUIRE(a->sourceBpm == 120.0);
    REQUIRE(a->filePath == "C:/audio/kick.wav");
    REQUIRE(a->waveform.size() == 2);
    REQUIRE(a->waveform[1] == 0.8f);
    REQUIRE(track.isAudioFree (5.0, 2.0));
    REQUIRE_FALSE(track.isAudioFree (4.0, 2.0));
    REQUIRE(track.isAudioFree (2.0, 3.0, a->id));
}

TEST_CASE("Dividir clip de audio preserva a origem e separa waveform e offset")
{
    Track track;
    track.type = TrackType::Audio;
    auto* original = track.addAudioClip (2.0, 8.0, "C:/audio/loop.wav", "loop.wav",
                                         { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f },
                                         4.0, 120.0);
    original->sourceOffsetSeconds = 1.5;
    original->fadeInBeats = 1.0;
    original->fadeOutBeats = 1.0;

    const int rightId = track.splitAudioClip (original->id, 6.0);
    REQUIRE(rightId > 0);
    REQUIRE(original->start == 2.0);
    REQUIRE(original->lengthBeats == 4.0);
    REQUIRE(original->durationSeconds == 2.0);
    REQUIRE(original->waveform.size() == 4);

    const auto* right = track.findAudioClip (rightId);
    REQUIRE(right != nullptr);
    REQUIRE(right->start == 6.0);
    REQUIRE(right->lengthBeats == 4.0);
    REQUIRE(right->durationSeconds == 2.0);
    REQUIRE(right->sourceOffsetSeconds == 3.5);
    REQUIRE(right->waveform.size() == 4);
    REQUIRE(right->filePath == original->filePath);
    REQUIRE(original->fadeInBeats == 1.0);
    REQUIRE(original->fadeOutBeats == 0.0);
    REQUIRE(right->fadeInBeats == 0.0);
    REQUIRE(right->fadeOutBeats == 1.0);
}

TEST_CASE("Copiar clip de audio preserva offset e fades")
{
    Track track;
    track.type = TrackType::Audio;
    auto* original = track.addAudioClip (0.0, 4.0, "C:/audio/loop.wav", "loop.wav",
                                         { 0.2f, 0.8f }, 2.0, 120.0);
    original->sourceOffsetSeconds = 0.5;
    original->fadeInBeats = 0.75;
    original->fadeOutBeats = 1.25;

    auto* copy = track.copyAudioClip (*original, 4.0);
    REQUIRE(copy != nullptr);
    REQUIRE(copy->start == 4.0);
    REQUIRE(copy->sourceOffsetSeconds == 0.5);
    REQUIRE(copy->fadeInBeats == 0.75);
    REQUIRE(copy->fadeOutBeats == 1.25);
    REQUIRE(copy->waveform == original->waveform);
    REQUIRE(original->start == 0.0);
    REQUIRE(track.copyAudioClip (*original, 2.0) == nullptr);
}

TEST_CASE("Dividir clips fora de seus limites nao altera a trilha")
{
    Track midi;
    auto* midiClip = midi.addClip (4.0, 4.0);
    REQUIRE(midi.splitClip (midiClip->id, 4.0) == -1);
    REQUIRE(midi.splitClip (midiClip->id, 8.0) == -1);
    REQUIRE(midi.clips.size() == 1);

    Track audio;
    audio.type = TrackType::Audio;
    auto* audioClip = audio.addAudioClip (4.0, 4.0, "C:/audio/one.wav", "one.wav");
    REQUIRE(audio.splitAudioClip (audioClip->id, 4.0) == -1);
    REQUIRE(audio.splitAudioClip (audioClip->id, 8.0) == -1);
    REQUIRE(audio.audioClips.size() == 1);
}

TEST_CASE("Mover clip de audio preserva seus metadados")
{
    Track source, destination;
    auto* clip = source.addAudioClip (1.0, 4.0, "C:/audio/voice.wav", "voice.wav", { 0.4f, 0.7f }, 2.0, 120.0);
    auto moved = source.takeAudioClip (clip->id);

    REQUIRE(source.audioClips.empty());
    destination.putAudioClip (std::move (moved));
    REQUIRE(destination.audioClips.size() == 1);
    REQUIRE(destination.audioClips.front()->filePath == "C:/audio/voice.wav");
    REQUIRE(destination.audioClips.front()->waveform.size() == 2);
    REQUIRE(destination.audioClips.front()->durationSeconds == 2.0);
    REQUIRE(destination.audioClips.front()->sourceBpm == 120.0);
}

TEST_CASE("Roteamento de saida da trilha padrao e Master e pode ser alterado para ExtOut")
{
    Track track;
    REQUIRE(track.outputRouting == TrackOutputRouting::Master);

    track.outputRouting = TrackOutputRouting::ExtOut;
    REQUIRE(track.outputRouting == TrackOutputRouting::ExtOut);
}

TEST_CASE("Trilhas iniciam com ganho unitario")
{
    Track track;
    REQUIRE(track.volume == 1.0);
    REQUIRE(std::abs(gainToDecibels(static_cast<float>(track.volume))) < 0.001f);
}

TEST_CASE("Conversao linear e decibeis com ganho unitario e menos infinito")
{
    // Ganho unitario 1.0 = 0.0 dB
    REQUIRE(std::abs(gainToDecibels(1.0f) - 0.0f) < 0.001f);
    REQUIRE(std::abs(decibelsToGain(0.0f) - 1.0f) < 0.001f);

    // +6.0 dB ~ 1.995
    REQUIRE(std::abs(decibelsToGain(6.0f) - 1.99526f) < 0.005f);
    REQUIRE(std::abs(gainToDecibels(1.99526f) - 6.0f) < 0.01f);

    // -6.0 dB ~ 0.501
    REQUIRE(std::abs(decibelsToGain(-6.0f) - 0.50118f) < 0.005f);
    REQUIRE(std::abs(gainToDecibels(0.50118f) - (-6.0f)) < 0.01f);

    // Silencio / -inf dB
    REQUIRE(gainToDecibels(0.0f) == -60.0f);
    REQUIRE(decibelsToGain(-60.0f) == 0.0f);
}

TEST_CASE("Copiar uma trilha preserva clips e IDs seguintes")
{
    Track original;
    original.id = 7;
    original.type = TrackType::Audio;
    auto* audio = original.addAudioClip (1.0, 2.0, "C:/audio/a.wav", "a.wav", { 0.5f });
    audio->sourceOffsetSeconds = 0.25;

    Track snapshot (original);
    REQUIRE(snapshot.id == 7);
    REQUIRE(snapshot.audioClips.size() == 1);
    REQUIRE(snapshot.audioClips.front()->id == audio->id);
    REQUIRE(snapshot.audioClips.front()->sourceOffsetSeconds == 0.25);
    REQUIRE(snapshot.addAudioClip (4.0, 1.0, "C:/audio/b.wav", "b.wav")->id == audio->id + 1);
}
