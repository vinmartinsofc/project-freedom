#include <catch2/catch_test_macros.hpp>
#include "Sequencer.h"
#include <cmath>
#include <cstdlib>

using namespace roll;

namespace
{
struct Abs { long long sample; bool on; int pitch; };

// Relógio + sequenciador em blocos de 512; devolve eventos com tempo absoluto em amostras.
std::vector<Abs> run(Sequencer& s, TransportClock& clock, const std::vector<Note>& notes,
                     const TransportSettings& t, double sr, long long totalSamples, int block = 512)
{
    std::vector<Abs> all;
    std::vector<MidiEvent> ev;
    BlockPlan plan;
    for (long long done = 0; done < totalSamples; done += block)
    {
        clock.plan(t, sr, block, plan);
        ev.clear();
        s.process(notes, plan, ev);
        for (auto& e : ev) all.push_back({ done + e.offset, e.on, e.pitch });
    }
    return all;
}
bool near(long long a, long long b, long long tol = 2) { return std::llabs(a - b) <= tol; }
}

TEST_CASE("Nota toca e para nos instantes certos (120 BPM, 44.1k)")
{
    Sequencer s; TransportClock c;
    TransportSettings t; t.loop = false;
    std::vector<Note> notes { { 60, 1.0, 1.0, 100 } };      // beat 1 a 2

    auto ev = run(s, c, notes, t, 44100.0, 66150);           // 3 beats = 1.5 s
    REQUIRE(ev.size() == 2);
    REQUIRE(ev[0].on);
    REQUIRE(near(ev[0].sample, 22050));                     // 1 beat = 22050 amostras
    REQUIRE_FALSE(ev[1].on);
    REQUIRE(near(ev[1].sample, 44100));
}

TEST_CASE("Loop corta a nota no fim e dispara de novo na volta")
{
    Sequencer s; TransportClock c;
    TransportSettings t; t.loop = true; t.loopStart = 0.0; t.loopEnd = 4.0;
    std::vector<Note> notes { { 62, 3.0, 2.0, 100 } };       // passa do fim do loop

    auto ev = run(s, c, notes, t, 44100.0, 176400 + 4096);   // 4 beats = 88200 amostras
    int ons = 0, offs = 0;
    for (auto& e : ev) { e.on ? ++ons : ++offs; }
    REQUIRE(ons == 2);
    REQUIRE(offs >= 2);
    for (auto& e : ev) if (!e.on) { REQUIRE(near(e.sample, 88200, 600)); break; }
}

TEST_CASE("Duas trilhas compartilham o mesmo relogio e ficam sincronizadas")
{
    Sequencer s1, s2; TransportClock c;
    TransportSettings t; t.loop = false;
    std::vector<Note> n1 { { 60, 1.0, 1.0, 100 } };
    std::vector<Note> n2 { { 64, 1.0, 1.0, 100 } };

    std::vector<MidiEvent> e1, e2;
    BlockPlan plan;
    long long first1 = -1, first2 = -1;
    for (long long done = 0; done < 44100; done += 512)
    {
        c.plan(t, 44100.0, 512, plan);
        e1.clear(); e2.clear();
        s1.process(n1, plan, e1);
        s2.process(n2, plan, e2);
        for (auto& e : e1) if (e.on && first1 < 0) first1 = done + e.offset;
        for (auto& e : e2) if (e.on && first2 < 0) first2 = done + e.offset;
    }
    REQUIRE(first1 > 0);
    REQUIRE(first1 == first2);
}

TEST_CASE("flush solta todas as notas ativas")
{
    Sequencer s; TransportClock c;
    TransportSettings t; t.loop = false;
    std::vector<Note> notes { { 60, 0.0, 8.0, 100 }, { 64, 0.0, 8.0, 100 } };
    std::vector<MidiEvent> ev;
    BlockPlan plan;
    c.plan(t, 44100.0, 512, plan);
    s.process(notes, plan, ev);
    REQUIRE(s.hasActiveNotes());
    std::uint64_t lo, hi;
    s.activeMask(lo, hi);
    REQUIRE(((lo >> 60) & 1) == 1);
    ev.clear();
    s.flush(0, ev);
    REQUIRE(ev.size() == 2);
    REQUIRE_FALSE(s.hasActiveNotes());
}

TEST_CASE("Nota desativada nao toca")
{
    Sequencer s; TransportClock c;
    TransportSettings t; t.loop = false;
    Note n { 60, 0.0, 1.0, 100 };
    n.muted = true;
    auto ev = run(s, c, { n }, t, 44100.0, 44100);
    REQUIRE(ev.empty());
}
