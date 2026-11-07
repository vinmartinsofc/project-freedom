#include "Sequencer.h"
#include <algorithm>
#include <cmath>

namespace roll
{
// ------------------------------------------------------------ TransportClock

void TransportClock::plan (const TransportSettings& t, double sampleRate, int numSamples, BlockPlan& out)
{
    out.count = 0;
    out.numSamples = numSamples;
    if (numSamples <= 0 || sampleRate <= 0.0 || t.bpm <= 0.0) return;

    const double bps = t.bpm / 60.0 / sampleRate;           // beats por amostra
    out.beatsPerSample = bps;
    const bool loopOk = t.loop && t.loopEnd > t.loopStart;

    int done = 0;
    while (done < numSamples && out.count < BlockPlan::maxSegments)
    {
        bool flushBefore = false;
        if (loopOk && pos >= t.loopEnd)
        {
            flushBefore = true;
            pos = t.loopStart;
        }

        const int remaining = numSamples - done;
        double segEnd = pos + remaining * bps;
        int segSamples = remaining;
        bool wrap = false;

        if (loopOk && segEnd > t.loopEnd)
        {
            wrap = true;
            segEnd = t.loopEnd;
            segSamples = std::clamp (static_cast<int> (std::ceil ((t.loopEnd - pos) / bps)), 1, remaining);
        }

        Segment& s = out.seg[out.count++];
        s.a = pos; s.b = segEnd; s.offset = done; s.samples = segSamples;
        s.flushBefore = flushBefore; s.flushAfter = wrap;

        done += segSamples;
        pos = wrap ? t.loopStart : pos + segSamples * bps;
    }
}

// ------------------------------------------------------------ Sequencer

void Sequencer::flush (int sampleOffset, std::vector<MidiEvent>& out)
{
    for (const auto& a : active)
        out.push_back ({ sampleOffset, false, a.pitch, 0 });
    active.clear();
}

void Sequencer::activeMask (std::uint64_t& lo, std::uint64_t& hi) const
{
    lo = hi = 0;
    for (const auto& a : active)
    {
        if (a.pitch < 64) lo |= (std::uint64_t { 1 } << a.pitch);
        else              hi |= (std::uint64_t { 1 } << (a.pitch - 64));
    }
}

void Sequencer::emit (const std::vector<Note>& notes, double a, double b,
                      int baseOffset, double bps, std::vector<MidiEvent>& out)
{
    auto toOffset = [&] (double beat)
    {
        return baseOffset + std::max (0, static_cast<int> (std::lround ((beat - a) / bps)));
    };

    for (const auto& n : notes)
    {
        if (n.muted || n.lengthBeats <= 1.0e-6) continue;
        if (n.startBeats >= a && n.startBeats < b)
        {
            out.push_back ({ toOffset (n.startBeats), true, n.pitch, n.velocity });
            active.push_back ({ n.id, n.pitch, n.endBeats() });
        }
    }

    for (size_t i = 0; i < active.size();)
    {
        if (active[i].end >= a && active[i].end < b)
        {
            out.push_back ({ toOffset (active[i].end), false, active[i].pitch, 0 });
            active.erase (active.begin() + static_cast<std::ptrdiff_t> (i));
        }
        else
            ++i;
    }
}

void Sequencer::process (const std::vector<Note>& notes, const BlockPlan& plan, std::vector<MidiEvent>& out)
{
    if (plan.count <= 0) return;
    const size_t firstNew = out.size();

    for (int i = 0; i < plan.count; ++i)
    {
        const Segment& s = plan.seg[i];
        if (s.flushBefore) flush (s.offset, out);
        emit (notes, s.a, s.b, s.offset, plan.beatsPerSample, out);
        if (s.flushAfter)  flush (s.offset + s.samples, out);
    }

    for (size_t i = firstNew; i < out.size(); ++i)
        out[i].offset = std::min (out[i].offset, plan.numSamples - 1);

    std::sort (out.begin() + static_cast<std::ptrdiff_t> (firstNew), out.end(),
               [] (const MidiEvent& x, const MidiEvent& y)
               {
                   if (x.offset != y.offset) return x.offset < y.offset;
                   return !x.on && y.on;                    // note-off primeiro
               });
}
} // namespace roll
