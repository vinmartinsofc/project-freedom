#include "ArrangementView.h"
#include "Icons.h"
#include <algorithm>
#include <cmath>

juce::Colour ArrangementView::colourFor (int trackId)
{
    return juce::Colour::fromHSV (std::fmod (static_cast<float> (trackId) * 0.137f + 0.45f, 1.0f), 0.45f, 0.78f, 1.0f);
}

// ============================================================ Lanes (clips)

class ArrangementView::Lanes : public juce::Component, public juce::DragAndDropTarget
{
public:
    Lanes (ProjectModel& p, ArrangementView& o) : project (p), owner (o) { setWantsKeyboardFocus (true); }

    void updateSize()
    {
        setSize (std::max (1, static_cast<int> (project.totalBeats * owner.ppb)),
                 std::max (1, static_cast<int> (project.tracks.size()) * trackHeight));
    }

    void focusGained (FocusChangeType) override { owner.repaint(); }
    void focusLost (FocusChangeType) override   { owner.repaint(); }

    void setPlayhead (double beats)
    {
        const int oldX = static_cast<int> (owner.playhead * owner.ppb);
        owner.playhead = beats;
        repaint (oldX - 2, 0, 5, getHeight());
        repaint (static_cast<int> (beats * owner.ppb) - 2, 0, 5, getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        const auto cb = g.getClipBounds();
        const double ppb = owner.ppb;

        for (size_t i = 0; i < project.tracks.size(); ++i)
        {
            g.setColour ((i % 2 == 0) ? theme::rowWhite() : theme::rowBlack());
            g.fillRect (cb.getX(), static_cast<int> (i) * trackHeight, cb.getWidth(), trackHeight);
            g.setColour (theme::border());
            g.drawHorizontalLine ((static_cast<int> (i) + 1) * trackHeight - 1, static_cast<float> (cb.getX()), static_cast<float> (cb.getRight()));
        }

        // região de loop
        const float lx0 = static_cast<float> (project.loopStart * ppb), lx1 = static_cast<float> (project.loopEnd * ppb);
        g.setColour (theme::accent().withAlpha (owner.loopEnabled ? 0.07f : 0.03f));
        g.fillRect (lx0, 0.0f, lx1 - lx0, static_cast<float> (getHeight()));

        // linhas de compasso/beat
        const int firstBeat = std::max (0, static_cast<int> (std::floor (cb.getX() / ppb)));
        const int lastBeat = static_cast<int> (std::ceil (cb.getRight() / ppb));
        for (int b = firstBeat; b <= lastBeat; ++b)
        {
            const bool bar = (b % 4 == 0);
            if (! bar && ppb < 14.0) continue;
            g.setColour (bar ? theme::border().brighter (0.5f) : theme::border().withAlpha (0.5f));
            g.drawVerticalLine (static_cast<int> (b * ppb), static_cast<float> (cb.getY()), static_cast<float> (cb.getBottom()));
        }

        // clips
        for (size_t i = 0; i < project.tracks.size(); ++i)
        {
            const auto& t = *project.tracks[i];
            const auto base = colourFor (t.id);
            for (const auto& p : t.clips)
            {
                juce::Rectangle<float> r (static_cast<float> (p->start * ppb), static_cast<float> (i) * trackHeight + 4.0f,
                                          static_cast<float> (p->clip.length() * ppb), static_cast<float> (trackHeight) - 9.0f);
                if (! r.toNearestInt().intersects (cb)) continue;

                const bool sel = (project.selectedTrackId == t.id && project.selectedClipId == p->id);
                g.setColour (sel ? base.brighter (0.25f) : base);
                g.fillRoundedRectangle (r, 4.0f);

                g.saveState();
                g.reduceClipRegion (r.toNearestInt());
                g.setColour (juce::Colours::black.withAlpha (0.28f));
                g.fillRect (r.withHeight (14.0f));
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (p->name.empty() ? juce::String ("Clip") : juce::String (p->name), r.toNearestInt().withTrimmedLeft (5).withHeight (14),
                            juce::Justification::centredLeft, true);

                const auto& notes = p->clip.notes();
                if (! notes.empty())
                {
                    int lo = 127, hi = 0;
                    for (const auto& n : notes) { lo = std::min (lo, n.pitch); hi = std::max (hi, n.pitch); }
                    const int range = std::max (hi - lo, 11) + 1;
                    const auto area = r.reduced (2.0f).withTrimmedTop (14.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.7f));
                    for (const auto& n : notes)
                    {
                        const float y = area.getBottom() - (static_cast<float> (n.pitch - lo) + 1.0f) / static_cast<float> (range) * area.getHeight();
                        g.fillRect (r.getX() + static_cast<float> (n.startBeats * ppb), y,
                                    std::max (1.5f, static_cast<float> (n.lengthBeats * ppb)), std::max (1.5f, area.getHeight() / static_cast<float> (range)));
                    }
                }
                g.restoreState();

                if (sel)
                {
                    g.setColour (juce::Colours::white);
                    g.drawRoundedRectangle (r, 4.0f, 1.6f);
                }
            }
        }

        g.setColour (theme::playhead());
        g.drawVerticalLine (static_cast<int> (owner.playhead * ppb), 0.0f, static_cast<float> (getHeight()));

        if (dragTrackIndex >= 0)
        {
            const auto colour = dragDropAllowed ? theme::accent() : juce::Colours::red;
            const int y = dragTrackIndex * trackHeight;
            g.setColour (colour.withAlpha (0.18f));
            g.fillRect (0, y, getWidth(), trackHeight);
            g.setColour (colour);
            g.drawRect (0, y, getWidth(), trackHeight, 2);

            if (dragDropAllowed)
            {
                const int markerX = static_cast<int> (dragBeat * ppb);
                g.drawVerticalLine (markerX, static_cast<float> (y), static_cast<float> (y + trackHeight));
            }

            const juce::String message = dragDropAllowed ? "Solte para adicionar plugin"
                                                         : "Plugin indisponível nesta trilha";
            const auto hint = juce::Rectangle<int> (8, y + 6, 220, 22);
            g.setColour (theme::panel().withAlpha (0.95f));
            g.fillRoundedRectangle (hint.toFloat(), 5.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (hint.toFloat(), 5.0f, 1.0f);
            g.setColour (theme::text());
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (message, hint.reduced (7, 0), juce::Justification::centredLeft, true);
        }

        drawActionHint (g);
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        // JUCE checks interest before converting localPosition to this target's coordinates.
        return details.description.toString().startsWith ("plugin:");
    }

    void itemDragEnter (const SourceDetails& details) override { updateDragTarget (details); }
    void itemDragMove (const SourceDetails& details) override  { updateDragTarget (details); }
    void itemDragExit (const SourceDetails&) override
    {
        dragTrackIndex = -1;
        dragDropAllowed = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void itemDropped (const SourceDetails& details) override
    {
        const auto description = details.description.toString();
        const auto trackIndex = trackIndexAt (details.localPosition.y);
        dragTrackIndex = -1;
        dragDropAllowed = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
        if (! description.startsWith ("plugin:")
            || trackIndex < 0
            || (owner.canAcceptPluginDrop
                && ! owner.canAcceptPluginDrop (description.substring (7),
                                                project.tracks[static_cast<std::size_t> (trackIndex)]->id)))
            return;

        const auto trackId = project.tracks[static_cast<std::size_t> (trackIndex)]->id;
        const auto beat = std::max (0.0, std::round (details.localPosition.x / owner.ppb));
        if (owner.onPluginDropped)
            owner.onPluginDropped (description.substring (7), trackId, beat);
    }

    // ---- mouse ----
    struct Hit { int trackIndex = -1; roll::Placement* clip = nullptr; };

    Hit hit (juce::Point<float> p) const
    {
        Hit h;
        h.trackIndex = static_cast<int> (std::floor (p.y / trackHeight));
        if (h.trackIndex < 0 || h.trackIndex >= static_cast<int> (project.tracks.size())) { h.trackIndex = -1; return h; }
        const double beat = p.x / owner.ppb;
        for (const auto& c : project.tracks[static_cast<size_t> (h.trackIndex)]->clips)
            if (beat >= c->start && beat < c->end()) { h.clip = c.get(); break; }
        return h;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        mode = Mode::None; changed = false; copied = false;

        const auto h = hit (e.position);
        if (h.trackIndex < 0) return;
        const int trackId = project.tracks[static_cast<size_t> (h.trackIndex)]->id;

        if (h.clip == nullptr)
        {
            project.selectTrack (trackId);
            return;
        }

        project.selectClip (trackId, h.clip->id);
        curTrack = trackId; curClip = h.clip->id;
        origTrack = trackId; origStart = h.clip->start; origLen = h.clip->clip.length();
        downBeat = e.position.x / owner.ppb;
        const float right = static_cast<float> (h.clip->end() * owner.ppb);
        mode = (e.position.x > right - 7.0f) ? Mode::Resize : Mode::Move;
        setMouseCursor (mode == Mode::Resize ? juce::MouseCursor::LeftRightResizeCursor
                                             : juce::MouseCursor::DraggingHandCursor);
    }

    void mouseMove (const juce::MouseEvent& e) override { updateCursor (e.position); }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (mode == Mode::None)
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (mode == Mode::None) return;
        const double beat = e.position.x / owner.ppb;
        const bool noSnap = e.mods.isAltDown();

        if (mode == Mode::Move)
        {
            double newStart = origStart + (beat - downBeat);
            newStart = std::max (0.0, noSnap ? newStart : std::round (newStart));

            const int ti = std::clamp (static_cast<int> (std::floor (e.position.y / trackHeight)), 0, static_cast<int> (project.tracks.size()) - 1);
            const int toTrack = project.tracks[static_cast<size_t> (ti)]->id;

            auto* cur = project.track (curTrack)->find (curClip);
            if (cur != nullptr && (toTrack != curTrack || std::abs (newStart - cur->start) > 1.0e-9))
            {
                const int nid = project.moveClip (curTrack, curClip, toTrack, newStart);
                if (nid >= 0)
                {
                    curTrack = toTrack; curClip = nid; changed = true;
                    if (e.mods.isCtrlDown() && ! copied)                 // Ctrl+arrastar: deixa uma cópia no lugar de origem
                    {
                        if (auto* src = project.track (origTrack))
                            if (auto* moved = project.track (curTrack)->find (curClip))
                                if (auto* c = src->isFree (origStart, origLen) ? src->addClip (origStart, origLen) : nullptr)
                                {
                                    c->clip.setNotes (moved->clip.notes());
                                    c->name = moved->name;
                                    project.notifyTrackNotes (origTrack);
                                }
                        copied = true;
                    }
                }
            }
        }
        else if (mode == Mode::Resize)
        {
            if (auto* t = project.track (curTrack))
                if (auto* p = t->find (curClip))
                {
                    double newLen = origLen + (beat - downBeat);
                    newLen = std::max (1.0, noSnap ? newLen : std::round (newLen));
                    if (std::abs (newLen - p->clip.length()) > 1.0e-9 && t->isFree (p->start, newLen, curClip))
                    {
                        p->clip.setLength (newLen);
                        project.growTo (p->end() + 64.0);
                        project.notifyTrackNotes (curTrack);
                        project.sendChangeMessage();
                        changed = true;
                    }
                }
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        mode = Mode::None;
        updateCursor (e.position);
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto h = hit (e.position);
        if (h.trackIndex < 0 || h.clip != nullptr) return;
        const int trackId = project.tracks[static_cast<size_t> (h.trackIndex)]->id;
        const double start = std::floor ((e.position.x / owner.ppb) / 4.0) * 4.0;
        project.addClip (trackId, start, 4.0);                         // 1 compasso
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown()) { owner.zoom (w.deltaY > 0 ? 1.15 : 1.0 / 1.15); return; }
        Component::mouseWheelMove (e, w);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();
        auto* t = project.selectedTrack();

        if (k.isKeyCode (juce::KeyPress::deleteKey) || k.isKeyCode (juce::KeyPress::backspaceKey))
        {
            if (t != nullptr && t->find (project.selectedClipId) != nullptr) { project.deleteClip (t->id, project.selectedClipId); return true; }
            return false;
        }

        if (ctrl && juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode())) == 'D')
        {
            if (auto* p = project.selectedClip())
            {
                const double len = p->clip.length();
                double start = p->end();
                for (int tries = 0; tries < 64 && ! t->isFree (start, len); ++tries) start += len;
                if (auto* c = project.addClip (t->id, start, len))
                {
                    c->clip.setNotes (p->clip.notes());
                    c->name = p->name;
                    project.notifyTrackNotes (t->id);
                    project.sendChangeMessage();
                }
                return true;
            }
            return false;
        }

        if (k.isKeyCode (juce::KeyPress::upKey) || k.isKeyCode (juce::KeyPress::downKey))
        {
            const int idx = project.trackIndex (project.selectedTrackId) + (k.isKeyCode (juce::KeyPress::upKey) ? -1 : 1);
            if (idx >= 0 && idx < static_cast<int> (project.tracks.size())) project.selectTrack (project.tracks[static_cast<size_t> (idx)]->id);
            return true;
        }

        const auto ch = k.getTextCharacter();
        if (! ctrl && (ch == '+' || ch == '=')) { owner.zoom (1.25);       return true; }
        if (! ctrl && (ch == '-' || ch == '_')) { owner.zoom (1.0 / 1.25); return true; }
        if (k.isKeyCode (juce::KeyPress::homeKey)) { if (owner.onSeek) owner.onSeek (0.0); return true; }
        return false;
    }

private:
    int trackIndexAt (int y) const
    {
        if (y < 0 || y >= static_cast<int> (project.tracks.size()) * trackHeight)
            return -1;
        return y / trackHeight;
    }

    void updateDragTarget (const SourceDetails& details)
    {
        const auto description = details.description.toString();
        const int target = trackIndexAt (details.localPosition.y);
        dragTrackIndex = target;
        dragBeat = std::max (0.0, std::round (details.localPosition.x / owner.ppb));
        dragDropAllowed = description.startsWith ("plugin:")
                       && target >= 0
                       && (! owner.canAcceptPluginDrop
                           || owner.canAcceptPluginDrop (
                               description.substring (7),
                               project.tracks[static_cast<std::size_t> (target)]->id));
        setMouseCursor (dragDropAllowed ? juce::MouseCursor::CopyingCursor
                                        : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void updateCursor (juce::Point<float> position)
    {
        if (mode == Mode::Resize)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }

        if (mode == Mode::Move)
        {
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }

        const auto h = hit (position);
        if (h.clip == nullptr)
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            return;
        }

        const float right = static_cast<float> (h.clip->end() * owner.ppb);
        setMouseCursor (position.x > right - 7.0f ? juce::MouseCursor::LeftRightResizeCursor
                                                   : juce::MouseCursor::DraggingHandCursor);
    }

    void drawActionHint (juce::Graphics& g)
    {
        if (mode == Mode::None)
            return;

        const auto* track = project.track (curTrack);
        const auto* clip = track != nullptr ? track->find (curClip) : nullptr;
        if (clip == nullptr)
            return;

        const juce::String message = mode == Mode::Resize
            ? "Tamanho: " + juce::String (clip->clip.length() / 4.0, 1) + " compassos"
            : "Movendo: compasso " + juce::String (static_cast<int> (std::floor (clip->start / 4.0)) + 1);
        const int x = std::clamp (static_cast<int> (clip->end() * owner.ppb) + 8, 4, std::max (4, getWidth() - 194));
        const int y = static_cast<int> (project.trackIndex (curTrack)) * trackHeight + 8;
        const auto hint = juce::Rectangle<int> (x, y, 190, 24);
        g.setColour (theme::panel().withAlpha (0.96f));
        g.fillRoundedRectangle (hint.toFloat(), 5.0f);
        g.setColour (theme::accent());
        g.drawRoundedRectangle (hint.toFloat(), 5.0f, 1.0f);
        g.setColour (theme::text());
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (message, hint.reduced (7, 0), juce::Justification::centredLeft, true);
    }

    enum class Mode { None, Move, Resize };
    ProjectModel& project;
    ArrangementView& owner;
    Mode mode = Mode::None;
    bool changed = false, copied = false;
    bool dragDropAllowed = false;
    int curTrack = -1, curClip = -1, origTrack = -1;
    int dragTrackIndex = -1;
    double origStart = 0.0, origLen = 4.0, downBeat = 0.0, dragBeat = 0.0;
};

// ============================================================ Ruler (régua + brace de loop)

class ArrangementView::Ruler : public juce::Component
{
public:
    Ruler (ProjectModel& p, ArrangementView& o) : project (p), owner (o) {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (theme::panelLight());
        const auto cb = g.getClipBounds();
        const double ppb = owner.ppb;

        // brace de loop
        const float lx0 = static_cast<float> (project.loopStart * ppb), lx1 = static_cast<float> (project.loopEnd * ppb);
        const auto braceColour = mode != Mode::Seek || hoverMode != Mode::Seek
                               ? theme::accent().brighter (0.2f)
                               : owner.loopEnabled ? theme::accent() : theme::textDim().withAlpha (0.6f);
        g.setColour (braceColour);
        g.fillRect (lx0, 0.0f, lx1 - lx0, 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (lx0, 0.0f, 3.0f, 8.0f);
        g.fillRect (lx1 - 3.0f, 0.0f, 3.0f, 8.0f);

        const bool leftActive = mode == Mode::LoopLeft || (mode == Mode::Seek && hoverMode == Mode::LoopLeft);
        const bool rightActive = mode == Mode::LoopRight || (mode == Mode::Seek && hoverMode == Mode::LoopRight);
        if (leftActive || rightActive)
        {
            g.setColour (juce::Colours::white);
            if (leftActive)
                g.fillRoundedRectangle (lx0 - 2.0f, 0.0f, 5.0f, 10.0f, 2.0f);
            if (rightActive)
                g.fillRoundedRectangle (lx1 - 3.0f, 0.0f, 5.0f, 10.0f, 2.0f);
        }

        const int labelStep = ppb < 10.0 ? 16 : ppb < 18.0 ? 8 : 4;          // beats entre rótulos
        g.setFont (juce::FontOptions (11.0f));
        const int firstBeat = std::max (0, static_cast<int> (std::floor (cb.getX() / ppb)) - 1);
        const int lastBeat = static_cast<int> (std::ceil (cb.getRight() / ppb));
        for (int b = firstBeat; b <= lastBeat; ++b)
        {
            const bool bar = (b % 4 == 0);
            if (! bar && ppb < 14.0) continue;
            const float x = static_cast<float> (b * ppb);
            g.setColour (bar ? theme::text() : theme::textDim());
            g.drawVerticalLine (static_cast<int> (x), bar ? 12.0f : 20.0f, static_cast<float> (getHeight()));
            if (b % labelStep == 0)
                g.drawText (juce::String (b / 4 + 1), static_cast<int> (x) + 3, 10, 40, getHeight() - 10, juce::Justification::centredLeft);
        }

        g.setColour (theme::playhead());
        juce::Path tri;
        const float px = static_cast<float> (owner.playhead * ppb);
        tri.addTriangle (px - 4.0f, 10.0f, px + 4.0f, 10.0f, px, 18.0f);
        g.fillPath (tri);

        if (mode == Mode::LoopLeft || mode == Mode::LoopRight || mode == Mode::LoopMove)
            drawActionHint (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mode = Mode::Seek;
        lastMouseX = e.position.x;
        if (e.y <= 10)
        {
            const double ppb = owner.ppb;
            const float lx0 = static_cast<float> (project.loopStart * ppb), lx1 = static_cast<float> (project.loopEnd * ppb);
            if (std::abs (e.position.x - lx0) < 6.0f)       mode = Mode::LoopLeft;
            else if (std::abs (e.position.x - lx1) < 6.0f)  mode = Mode::LoopRight;
            else if (e.position.x > lx0 && e.position.x < lx1) { mode = Mode::LoopMove; grabOffset = e.position.x / ppb - project.loopStart; }
        }
        hoverMode = mode;
        updateCursor (e.position);
        if (mode == Mode::Seek && owner.onSeek) owner.onSeek (std::max (0.0, e.position.x / owner.ppb));
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        lastMouseX = e.position.x;
        if (mode == Mode::Seek)
        {
            hoverMode = modeAt (e.position);
            updateCursor (e.position);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (mode == Mode::Seek)
        {
            hoverMode = Mode::Seek;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        lastMouseX = e.position.x;
        const double beat = std::max (0.0, std::round (e.position.x / owner.ppb));
        switch (mode)
        {
            case Mode::Seek:      if (owner.onSeek) owner.onSeek (std::max (0.0, e.position.x / owner.ppb)); break;
            case Mode::LoopLeft:  project.setLoop (std::min (beat, project.loopEnd - 1.0), project.loopEnd); break;
            case Mode::LoopRight: project.setLoop (project.loopStart, std::max (beat, project.loopStart + 1.0)); break;
            case Mode::LoopMove:
            {
                const double len = project.loopEnd - project.loopStart;
                const double s = std::max (0.0, std::round (e.position.x / owner.ppb - grabOffset));
                project.setLoop (s, s + len);
                break;
            }
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        mode = Mode::Seek;
        hoverMode = modeAt (e.position);
        updateCursor (e.position);
        repaint();
    }

private:
    enum class Mode { Seek, LoopLeft, LoopRight, LoopMove };
    ProjectModel& project;
    ArrangementView& owner;
    Mode mode = Mode::Seek;
    Mode hoverMode = Mode::Seek;
    double grabOffset = 0.0;
    float lastMouseX = 0.0f;

    Mode modeAt (juce::Point<float> position) const
    {
        if (position.y > 10.0f)
            return Mode::Seek;

        const float left = static_cast<float> (project.loopStart * owner.ppb);
        const float right = static_cast<float> (project.loopEnd * owner.ppb);
        if (std::abs (position.x - left) < 6.0f)  return Mode::LoopLeft;
        if (std::abs (position.x - right) < 6.0f) return Mode::LoopRight;
        if (position.x > left && position.x < right) return Mode::LoopMove;
        return Mode::Seek;
    }

    void updateCursor (juce::Point<float> position)
    {
        const auto target = mode == Mode::Seek ? modeAt (position) : mode;
        if (target == Mode::LoopLeft || target == Mode::LoopRight)
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else if (target == Mode::LoopMove)
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    static juce::String formatPosition (double beat)
    {
        const int wholeBeat = std::max (0, static_cast<int> (std::round (beat)));
        return juce::String (wholeBeat / 4 + 1) + "."
             + juce::String (wholeBeat % 4 + 1) + ".1";
    }

    void drawActionHint (juce::Graphics& g) const
    {
        juce::String message;
        if (mode == Mode::LoopLeft)
            message = "Início do loop: " + formatPosition (project.loopStart);
        else if (mode == Mode::LoopRight)
            message = "Fim do loop: " + formatPosition (project.loopEnd);
        else
            message = "Loop: " + formatPosition (project.loopStart) + " - "
                    + formatPosition (project.loopEnd) + "  ("
                    + juce::String ((project.loopEnd - project.loopStart) / 4.0, 1) + " compassos)";

        const auto hint = juce::Rectangle<int> (
            std::clamp (static_cast<int> (lastMouseX) + 12, 4, std::max (4, getWidth() - 274)),
            2, 270, 24);
        g.setColour (theme::panel().withAlpha (0.97f));
        g.fillRoundedRectangle (hint.toFloat(), 5.0f);
        g.setColour (theme::accent());
        g.drawRoundedRectangle (hint.toFloat(), 5.0f, 1.0f);
        g.setColour (theme::text());
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (message, hint.reduced (7, 0), juce::Justification::centredLeft, true);
    }
};

// ============================================================ Header (cabeçalho de trilha)

class ArrangementView::Header : public juce::Component
{
public:
    Header (ProjectModel& p, ArrangementView& o, int id) : project (p), owner (o), trackId (id)
    {
        auto* t = project.track (trackId);
        addAndMakeVisible (name);
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
        addAndMakeVisible (volume);

        name.setText (juce::String (t->name), juce::dontSendNotification);
        name.setEditable (false, true, false);
        name.setColour (juce::Label::textColourId, theme::text());
        name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        name.onTextChange = [this]
        {
            if (auto* tr = project.track (trackId)) tr->name = name.getText().toStdString();
        };

        for (auto* b : { &mute, &solo })
        {
            b->setClickingTogglesState (true);
            b->setMouseClickGrabsKeyboardFocus (false);
        }
        solo.setColour (juce::TextButton::buttonOnColourId, theme::accent());
        mute.setColour (juce::TextButton::buttonOnColourId, theme::accent());
        mute.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        solo.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        mute.setTooltip ("Silenciar trilha");
        solo.setTooltip ("Solo da trilha");
        mute.setToggleState (t->mute, juce::dontSendNotification);
        solo.setToggleState (t->solo, juce::dontSendNotification);
        mute.onClick = [this] { if (auto* tr = project.track (trackId)) { tr->mute = mute.getToggleState(); project.notifyMixer(); } };
        solo.onClick = [this] { if (auto* tr = project.track (trackId)) { tr->solo = solo.getToggleState(); project.notifyMixer(); } };

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setTooltip ("Volume da trilha");
        volume.setRange (0.0, 1.5, 0.001);
        volume.setValue (t->volume, juce::dontSendNotification);
        volume.setMouseClickGrabsKeyboardFocus (false);
        volume.onValueChange = [this] { if (auto* tr = project.track (trackId)) { tr->volume = volume.getValue(); project.notifyMixer(); } };
        volume.setDoubleClickReturnValue (true, 0.8);
    }

    int id() const { return trackId; }
    void setPeak (float p) { if (std::abs (p - peak) > 0.004f) { peak = p; repaint (getWidth() - 16, 0, 16, getHeight()); } }
    void refreshText() { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const bool sel = (project.selectedTrackId == trackId);
        g.setColour (sel ? theme::panelLight().brighter (0.15f) : theme::panel());
        g.fillRect (getLocalBounds());
        g.setColour (colourFor (trackId));
        g.fillRect (0, 0, 6, getHeight());
        g.setColour (theme::border());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

        g.setColour (theme::textDim());
        g.setFont (juce::FontOptions (11.0f));
        const juce::String summary = owner.deviceSummary ? owner.deviceSummary (trackId) : juce::String();
        g.drawText (summary.isEmpty() ? theme::ui ("sem instrumento") : summary, 14, 26, getWidth() - 30, 16, juce::Justification::centredLeft, true);

        if (const auto* volumeIcon = icons::get (icons::Icon::Volume, theme::textDim(), 16))
            volumeIcon->drawWithin (g, volumeIconBounds.toFloat(), juce::RectanglePlacement::centred, 1.0f);

        const float h = static_cast<float> (getHeight() - 12);
        const float level = std::min (1.0f, peak);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (static_cast<float> (getWidth() - 12), 6.0f, 6.0f, h);
        g.setColour (level > 0.9f ? theme::warning() : theme::accent());
        g.fillRect (static_cast<float> (getWidth() - 12), 6.0f + h * (1.0f - level), 6.0f, h * level);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (6, 4);
        a.removeFromLeft (8);
        a.removeFromRight (14);
        name.setBounds (a.removeFromTop (22));
        a.removeFromTop (18);
        auto row = a.removeFromTop (24);
        mute.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (4);
        solo.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (6);
        volumeIconBounds = row.removeFromLeft (20);
        volume.setBounds (row);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        project.selectTrack (trackId);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, theme::ui ("Nova trilha MIDI"));
            m.addItem (2, theme::ui ("Apagar trilha"), project.tracks.size() > 1);
            const int tid = trackId;
            auto* proj = &project;
            m.showMenuAsync ({}, [proj, tid] (int r)
            {
                if (r == 1) proj->addTrack();
                if (r == 2) proj->removeTrack (tid);
            });
        }
    }

private:
    ProjectModel& project;
    ArrangementView& owner;
    int trackId;
    juce::Rectangle<int> volumeIconBounds;
    juce::Label name;
    juce::TextButton mute { "M" }, solo { "S" };
    juce::Slider volume;
    float peak = 0.0f;
};

// ============================================================ ArrangementView

ArrangementView::ArrangementView (ProjectModel& p) : project (p)
{
    lanes = std::make_unique<Lanes> (project, *this);
    ruler = std::make_unique<Ruler> (project, *this);

    addAndMakeVisible (viewport);
    addAndMakeVisible (headerHolder);
    addAndMakeVisible (rulerHolder);
    addAndMakeVisible (addTrackButton);
    rulerHolder.addAndMakeVisible (*ruler);

    viewport.setViewedComponent (lanes.get(), false);
    viewport.setScrollBarsShown (true, true);

    addTrackButton.setMouseClickGrabsKeyboardFocus (false);
    addTrackButton.onClick = [this] { project.addTrack(); };

    project.addChangeListener (this);
    refresh();
    startTimerHz (30);
}

ArrangementView::~ArrangementView()
{
    stopTimer();
    project.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void ArrangementView::grabFocus() { lanes->grabKeyboardFocus(); }

void ArrangementView::zoom (double factor)
{
    ppb = std::clamp (ppb * factor, 4.0, 120.0);
    updateSizes();
    lanes->repaint();
    ruler->repaint();
}

void ArrangementView::setPlayhead (double beats)
{
    const int oldX = static_cast<int> (playhead * ppb);
    lanes->setPlayhead (beats);
    ruler->repaint (oldX - 6, 0, 14, rulerHeight);
    ruler->repaint (static_cast<int> (beats * ppb) - 6, 0, 14, rulerHeight);
}

void ArrangementView::setLoopEnabled (bool on)
{
    loopEnabled = on;
    lanes->repaint();
    ruler->repaint();
}

void ArrangementView::refresh()
{
    for (auto& h : headers) headerHolder.removeChildComponent (h.get());
    headers.clear();
    for (const auto& t : project.tracks)
    {
        headers.push_back (std::make_unique<Header> (project, *this, t->id));
        headerHolder.addAndMakeVisible (*headers.back());
    }
    updateSizes();
    resized();
}

void ArrangementView::updateSizes()
{
    lanes->updateSize();
    ruler->setSize (std::max (1, lanes->getWidth()), rulerHeight);
    syncPositions();
}

void ArrangementView::syncPositions()
{
    const auto vp = viewport.getViewPosition();
    ruler->setTopLeftPosition (-vp.x, 0);
    headerHolder.setSize (headerWidth, viewport.getMaximumVisibleHeight());
    for (size_t i = 0; i < headers.size(); ++i)
        headers[i]->setBounds (0, static_cast<int> (i) * trackHeight - vp.y, headerWidth, trackHeight);
}

void ArrangementView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (headers.size() != project.tracks.size()) { refresh(); }
    else
    {
        updateSizes();
        for (auto& h : headers) h->refreshText();
    }
    lanes->repaint();
    ruler->repaint();
}

void ArrangementView::timerCallback()
{
    const auto vp = viewport.getViewPosition();
    static juce::Point<int> last;
    if (vp != last) { last = vp; syncPositions(); }

    if (meterProvider)
        for (auto& h : headers) h->setPeak (meterProvider (h->id()));
}

void ArrangementView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    if (lanes->hasKeyboardFocus (false))
    {
        g.setColour (theme::accent());
        g.drawRect (getLocalBounds(), 2);
    }
}

void ArrangementView::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (rulerHeight);
    addTrackButton.setBounds (top.removeFromLeft (headerWidth).reduced (4));
    rulerHolder.setBounds (top);
    auto left = area.removeFromLeft (headerWidth);
    viewport.setBounds (area);
    headerHolder.setBounds (left.withHeight (viewport.getMaximumVisibleHeight()));
    updateSizes();
}
