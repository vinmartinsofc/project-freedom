#include "Arrangement.h"
#include <algorithm>

namespace roll
{
Placement* Track::addClip (double start, double length)
{
    auto p = std::make_unique<Placement>();
    p->id = nextId++;
    p->start = start;
    p->clip.setLength (length);
    clips.push_back (std::move (p));
    return clips.back().get();
}

Placement* Track::find (int clipId) const
{
    for (const auto& c : clips) if (c->id == clipId) return c.get();
    return nullptr;
}

std::unique_ptr<Placement> Track::take (int clipId)
{
    for (auto it = clips.begin(); it != clips.end(); ++it)
        if ((*it)->id == clipId)
        {
            auto p = std::move (*it);
            clips.erase (it);
            return p;
        }
    return nullptr;
}

void Track::put (std::unique_ptr<Placement> p)
{
    p->id = nextId++;                  // id novo na trilha de destino
    clips.push_back (std::move (p));
}

bool Track::remove (int clipId) { return take (clipId) != nullptr; }

bool Track::isFree (double start, double length, int ignoreId) const
{
    constexpr double eps = 1.0e-9;
    for (const auto& c : clips)
    {
        if (c->id == ignoreId) continue;
        if (start < c->end() - eps && start + length > c->start + eps) return false;
    }
    return true;
}

std::vector<Note> flatten (const Track& track)
{
    std::vector<Note> out;
    int id = 1;
    for (const auto& p : track.clips)
        for (const auto& n : p->clip.notes())
        {
            if (n.startBeats >= p->clip.length()) continue;
            Note a = n;
            a.startBeats = p->start + n.startBeats;
            a.lengthBeats = std::min (n.lengthBeats, p->clip.length() - n.startBeats);
            a.id = id++;
            out.push_back (a);
        }
    return out;
}
} // namespace roll
