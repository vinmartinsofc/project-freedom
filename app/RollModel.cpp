#include "RollModel.h"
#include <algorithm>
#include <cmath>
#include <limits>

RollModel::RollModel() { rebuildRows(); }

void RollModel::setClip (roll::Clip* c)
{
    clipPtr = (c != nullptr) ? c : &dummyClip;
    selection.clear();
    history.clear();
    insertBeat = 0.0;
    rebuildRows();
    sendChangeMessage();
}

void RollModel::rebuildRows()
{
    const bool folding = fold || (foldScale && scaleOn);
    std::vector<bool> show (128, ! folding);

    if (folding)
    {
        for (const auto& n : clip().notes())
            show[static_cast<size_t> (n.pitch)] = true;
        if (foldScale && scaleOn)
            for (int p = 0; p < 128; ++p)
                if (scale.contains (p)) show[static_cast<size_t> (p)] = true;
        if (std::none_of (show.begin(), show.end(), [] (bool b) { return b; }))
            std::fill (show.begin(), show.end(), true);
    }

    rowPitches.clear();
    for (int p = 127; p >= 0; --p)
        if (show[static_cast<size_t> (p)])
            rowPitches.push_back (p);
}

int RollModel::pitchToRow (int pitch) const
{
    auto it = std::find (rowPitches.begin(), rowPitches.end(), pitch);
    return it == rowPitches.end() ? -1 : static_cast<int> (it - rowPitches.begin());
}

int RollModel::rowToPitch (int row) const
{
    if (row < 0 || row >= static_cast<int> (rowPitches.size())) return -1;
    return rowPitches[static_cast<size_t> (row)];
}

juce::String RollModel::gridLabel() const
{
    const double denom = 4.0 / baseGrid;            // 0.25 beat -> 1/16
    juce::String text = denom < 1.0 ? juce::String ("1 bar+") : "1/" + juce::String (static_cast<int> (std::lround (denom)));
    if (triplet) text += "T";
    if (! snap)  text += " (snap off)";
    return text;
}

void RollModel::beginEdit() { history.record (clip().notes()); }

void RollModel::notify (bool rebuildRowLayout)
{
    if (rebuildRowLayout) rebuildRows();
    if (onNotesChanged) onNotesChanged();
    sendChangeMessage();
}

void RollModel::pruneSelection()
{
    for (auto it = selection.begin(); it != selection.end();)
        it = (clip().find (*it) == nullptr) ? selection.erase (it) : std::next (it);
}

void RollModel::undo()
{
    auto current = clip().notes();
    if (history.undo (current)) { clip().setNotes (std::move (current)); pruneSelection(); notify (true); }
}

void RollModel::redo()
{
    auto current = clip().notes();
    if (history.redo (current)) { clip().setNotes (std::move (current)); pruneSelection(); notify (true); }
}

void RollModel::selectAll()
{
    selection.clear();
    for (const auto& n : clip().notes()) selection.insert (n.id);
    sendChangeMessage();
}

void RollModel::deleteSelected()
{
    if (selection.empty()) return;
    beginEdit();
    for (int id : selection) clip().removeNote (id);
    selection.clear();
    notify (true);
}

void RollModel::duplicateSelected()
{
    if (selection.empty()) return;

    double minStart = std::numeric_limits<double>::max();
    double maxEnd   = std::numeric_limits<double>::lowest();
    for (int id : selection)
        if (auto* n = clip().find (id)) { minStart = std::min (minStart, n->startBeats); maxEnd = std::max (maxEnd, n->endBeats()); }

    const double shift = std::max (gridBeats, std::ceil ((maxEnd - minStart) / gridBeats - 1.0e-9) * gridBeats);

    beginEdit();
    std::set<int> newSelection;
    const auto sources = std::vector<int> (selection.begin(), selection.end());
    for (int id : sources)
        if (auto* n = clip().find (id))
        {
            roll::Note copy = *n;
            copy.startBeats += shift;
            newSelection.insert (clip().addNote (copy));
        }

    double newEnd = 0.0;
    for (int id : newSelection) if (auto* n = clip().find (id)) newEnd = std::max (newEnd, n->endBeats());
    extendToFit (newEnd);

    selection = newSelection;
    resolveOverlaps (selection);
    notify (true);
}

void RollModel::nudgeSelected (double dBeats, int dPitch)
{
    if (selection.empty()) return;

    for (int id : selection)
        if (auto* n = clip().find (id))
        {
            if (n->startBeats + dBeats < 0.0) return;
            if (n->pitch + dPitch < 0 || n->pitch + dPitch > 127) return;
        }

    beginEdit();
    for (int id : selection)
        if (auto* n = clip().find (id)) { n->startBeats += dBeats; n->pitch += dPitch; }
    resolveOverlaps (selection);
    notify (true);
}

void RollModel::quantizeSelected()
{
    if (selection.empty()) return;
    beginEdit();
    for (int id : selection)
        if (auto* n = clip().find (id)) n->startBeats = std::max (0.0, roll::snapToGrid (n->startBeats, gridBeats));
    notify (false);
}

void RollModel::toggleMuteSelected()
{
    if (selection.empty()) return;
    bool anyActive = false;
    for (int id : selection) if (auto* n = clip().find (id)) anyActive |= ! n->muted;

    beginEdit();
    for (int id : selection) if (auto* n = clip().find (id)) n->muted = anyActive;
    notify (false);
}

void RollModel::updateGrid()     { gridBeats = baseGrid * (triplet ? 2.0 / 3.0 : 1.0); }
void RollModel::narrowGrid()     { baseGrid = std::max (1.0 / 16.0, baseGrid * 0.5); updateGrid(); sendChangeMessage(); }
void RollModel::widenGrid()      { baseGrid = std::min (4.0, baseGrid * 2.0);        updateGrid(); sendChangeMessage(); }
void RollModel::toggleSnap()     { snap = ! snap;                                    sendChangeMessage(); }
void RollModel::toggleTriplet()  { triplet = ! triplet;                              updateGrid(); sendChangeMessage(); }

void RollModel::extendToFit (double endBeat)
{
    if (endBeat > clip().length())
    {
        clip().setLength (std::ceil (endBeat / 4.0) * 4.0);
        if (onLengthChanged) onLengthChanged();
    }
}

void RollModel::resizeSelected (double dBeats)
{
    if (selection.empty()) return;
    beginEdit();
    for (int id : selection)
        if (auto* n = clip().find (id)) n->lengthBeats = std::max (minLength(), n->lengthBeats + dBeats);
    resolveOverlaps (selection);
    notify (false);
}

void RollModel::resolveOverlaps (const std::set<int>& ids)
{
    clip().resolveOverlaps (std::vector<int> (ids.begin(), ids.end()));
    pruneSelection();
}

void RollModel::copySelected (bool cut)
{
    if (selection.empty()) return;
    double minStart = std::numeric_limits<double>::max();
    for (int id : selection) if (auto* n = clip().find (id)) minStart = std::min (minStart, n->startBeats);

    clipboard.clear();
    for (int id : selection)
        if (auto* n = clip().find (id)) { roll::Note c = *n; c.startBeats -= minStart; clipboard.push_back (c); }

    if (cut) deleteSelected();
}

void RollModel::paste()
{
    if (clipboard.empty()) return;
    beginEdit();
    std::set<int> pasted;
    double newEnd = 0.0;
    for (auto n : clipboard)
    {
        n.startBeats += insertBeat;
        newEnd = std::max (newEnd, n.endBeats());
        pasted.insert (clip().addNote (n));
    }
    extendToFit (newEnd);
    selection = pasted;
    resolveOverlaps (selection);
    notify (true);
}

void RollModel::setInsertMarker (double beats)
{
    insertBeat = std::clamp (beats, 0.0, clip().length());
    if (onInsertMarker) onInsertMarker (insertBeat);
    sendChangeMessage();
}
