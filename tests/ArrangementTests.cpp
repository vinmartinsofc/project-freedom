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
