#include "ArrangementView.h"
#include "Icons.h"
#include <algorithm>
#include <cmath>
#include <utility>

juce::Colour ArrangementView::colourFor (int trackId)
{
    return juce::Colour::fromHSV (std::fmod (static_cast<float> (trackId) * 0.137f + 0.45f, 1.0f), 0.45f, 0.78f, 1.0f);
}

// ============================================================ Lanes (clips)

class ArrangementView::Lanes : public juce::Component, public juce::DragAndDropTarget,
                               public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    enum class Mode { None, Move, Resize, FadeIn, FadeOut, RangeSelect };

    Lanes (ProjectModel& p, ArrangementView& o) : project (p), owner (o)
    {
        setWantsKeyboardFocus (true);
        startTimerHz (30);
    }

    void timerCallback() override
    {
        if (mode == Mode::FadeIn || mode == Mode::FadeOut || mode == Mode::RangeSelect)
            repaint();
    }

    void updateSize()
    {
        setSize (std::max (1, static_cast<int> (project.totalBeats * owner.pixelsPerBeat)),
                 std::max (1, static_cast<int> (project.tracks.size() + 1) * trackHeight));
    }

    void focusGained (FocusChangeType) override { owner.repaint(); }
    void focusLost (FocusChangeType) override   { owner.repaint(); }

    void setPlayhead (double beats)
    {
        const int oldX = static_cast<int> (owner.playhead * owner.pixelsPerBeat);
        owner.playhead = beats;
        repaint (oldX - 2, 0, 5, getHeight());
        repaint (static_cast<int> (beats * owner.pixelsPerBeat) - 2, 0, 5, getHeight());
    }

    void paint (juce::Graphics& g) override
    {
        const auto cb = g.getClipBounds();
        const double ppb = owner.pixelsPerBeat;

        const size_t totalRows = project.tracks.size() + 1;
        for (size_t i = 0; i < totalRows; ++i)
        {
            if (i == project.tracks.size())
            {
                g.setColour (theme::panel().darker (0.2f));
                g.fillRect (cb.getX(), static_cast<int> (i) * trackHeight, cb.getWidth(), trackHeight);
                g.setColour (theme::textDim().withAlpha (0.45f));
                g.setFont (juce::FontOptions (11.0f));
                g.drawText (theme::ui ("Master Bus  (Saida Estereo 1/2)"), cb.getX() + 12, static_cast<int> (i) * trackHeight + 4, 300, 20, juce::Justification::centredLeft);
            }
            else
            {
                g.setColour ((i % 2 == 0) ? theme::rowWhite() : theme::rowBlack());
                g.fillRect (cb.getX(), static_cast<int> (i) * trackHeight, cb.getWidth(), trackHeight);
            }
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
        const double grid = owner.gridBeats();
        if (grid < 1.0 && grid * ppb >= 2.0)
        {
            const int firstGrid = std::max (0, static_cast<int> (std::floor (cb.getX() / (grid * ppb))));
            const int lastGrid = static_cast<int> (std::ceil (cb.getRight() / (grid * ppb)));
            for (int i = firstGrid; i <= lastGrid; ++i)
            {
                const double beat = i * grid;
                if (std::abs (std::remainder (beat, 1.0)) < 1.0e-6)
                    continue;
                g.setColour (theme::border().withAlpha (0.25f));
                g.drawVerticalLine (static_cast<int> (beat * ppb), static_cast<float> (cb.getY()),
                                    static_cast<float> (cb.getBottom()));
            }
        }

        // clips
        for (size_t i = 0; i < project.tracks.size(); ++i)
        {
            const auto& t = *project.tracks[i];
            const auto base = colourFor (t.id);
            if (t.type == roll::TrackType::Audio)
            {
                for (const auto& clip : t.audioClips)
                {
                    juce::Rectangle<float> r (static_cast<float> (clip->start * ppb), static_cast<float> (i) * trackHeight + 4.0f,
                                              static_cast<float> (clip->lengthBeats * ppb), static_cast<float> (trackHeight) - 9.0f);
                    if (! r.toNearestInt().intersects (cb)) continue;

                    const bool inRange = hasRangeSelection && static_cast<int> (i) >= std::min (rangeStartTrack, rangeEndTrack)
                        && static_cast<int> (i) <= std::max (rangeStartTrack, rangeEndTrack)
                        && clip->start >= std::min (rangeStartBeat, rangeEndBeat) - 1.0e-9
                        && clip->end() <= std::max (rangeStartBeat, rangeEndBeat) + 1.0e-9;
                    const bool selected = (project.selectedTrackId == t.id && project.selectedAudioClipId == clip->id) || inRange;
                    g.setColour (selected ? base.brighter (0.25f) : base);
                    g.fillRoundedRectangle (r, 4.0f);
                    g.saveState();
                    g.reduceClipRegion (r.toNearestInt());
                    g.setColour (juce::Colours::black.withAlpha (0.28f));
                    g.fillRect (r.withHeight (14.0f));
                    g.setColour (juce::Colours::black.withAlpha (0.8f));
                    g.setFont (juce::FontOptions (11.0f));
                    g.drawText (clip->name, r.toNearestInt().withTrimmedLeft (5).withHeight (14),
                                juce::Justification::centredLeft, true);

                    const auto waveformArea = r.withY (r.getY() + 15.0f).withHeight (r.getHeight() - 15.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.3f));
                    g.fillRect (waveformArea);
                    const float center = r.getY() + 42.0f;
                    const float halfHeight = 24.0f;
                    g.setColour (juce::Colours::white.withAlpha (0.24f));
                    g.drawHorizontalLine (static_cast<int> (center), waveformArea.getX(), waveformArea.getRight());
                    g.setColour (base.brighter (0.7f).withAlpha (0.9f));
                    for (size_t bin = 0; bin < clip->waveform.size(); ++bin)
                    {
                        const float x = r.getX() + static_cast<float> (bin) / static_cast<float> (clip->waveform.size())
                                                    * r.getWidth();
                        const float amplitude = std::clamp (clip->waveform[bin], 0.0f, 1.0f) * halfHeight;
                        g.drawVerticalLine (static_cast<int> (x), center - amplitude, center + amplitude);
                    }

                    const float fadeTop = waveformArea.getY() + 3.0f;
                    const float fadeBottom = waveformArea.getBottom() - 3.0f;
                    const float fadeHandleInset = std::min (3.0f, r.getWidth() * 0.5f);
                    const float fadeInX = std::clamp (r.getX() + static_cast<float> (clip->fadeInBeats * ppb),
                                                      r.getX() + fadeHandleInset, r.getRight() - fadeHandleInset);
                    const float fadeOutX = std::clamp (r.getRight() - static_cast<float> (clip->fadeOutBeats * ppb),
                                                       r.getX() + fadeHandleInset, r.getRight() - fadeHandleInset);
                    const bool editingFadeIn = mode == Mode::FadeIn
                                            && curTrack == t.id && curAudioClipId == clip->id;
                    const bool editingFadeOut = mode == Mode::FadeOut
                                             && curTrack == t.id && curAudioClipId == clip->id;
                    const float pulse = 0.65f + 0.35f * std::sin (
                        static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.008));
                    g.setColour (theme::accent().brighter (0.5f).withAlpha (
                        editingFadeIn || editingFadeOut ? pulse : 0.9f));
                    juce::Path fadeInPath;
                    fadeInPath.startNewSubPath (r.getX(), fadeBottom);
                    fadeInPath.lineTo (fadeInX, fadeTop);
                    g.strokePath (fadeInPath, juce::PathStrokeType (editingFadeIn ? 2.0f + pulse : 2.0f));
                    juce::Path fadeOutPath;
                    fadeOutPath.startNewSubPath (fadeOutX, fadeTop);
                    fadeOutPath.lineTo (r.getRight(), fadeBottom);
                    g.strokePath (fadeOutPath, juce::PathStrokeType (editingFadeOut ? 2.0f + pulse : 2.0f));
                    const float inRadius = editingFadeIn ? 3.0f + pulse : 3.0f;
                    const float outRadius = editingFadeOut ? 3.0f + pulse : 3.0f;
                    g.fillEllipse (fadeInX - inRadius, fadeTop - inRadius, inRadius * 2.0f, inRadius * 2.0f);
                    g.fillEllipse (fadeOutX - outRadius, fadeTop - outRadius, outRadius * 2.0f, outRadius * 2.0f);
                    g.restoreState();
                    if (selected)
                    {
                        g.setColour (juce::Colours::white);
                        g.drawRoundedRectangle (r, 4.0f, 1.6f);
                    }
                }
                continue;
            }

            for (const auto& p : t.clips)
            {
                juce::Rectangle<float> r (static_cast<float> (p->start * ppb), static_cast<float> (i) * trackHeight + 4.0f,
                                          static_cast<float> (p->clip.length() * ppb), static_cast<float> (trackHeight) - 9.0f);
                if (! r.toNearestInt().intersects (cb)) continue;

                const bool inRange = hasRangeSelection && static_cast<int> (i) >= std::min (rangeStartTrack, rangeEndTrack)
                    && static_cast<int> (i) <= std::max (rangeStartTrack, rangeEndTrack)
                    && p->start >= std::min (rangeStartBeat, rangeEndBeat) - 1.0e-9
                    && p->end() <= std::max (rangeStartBeat, rangeEndBeat) + 1.0e-9;
                const bool sel = (project.selectedTrackId == t.id && project.selectedClipId == p->id) || inRange;
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

        if (hasRangeSelection)
        {
            const double startBeat = std::min (rangeStartBeat, rangeEndBeat);
            const double endBeat = std::max (rangeStartBeat, rangeEndBeat);
            const int firstTrack = std::min (rangeStartTrack, rangeEndTrack);
            const int lastTrack = std::max (rangeStartTrack, rangeEndTrack);
            const float x = static_cast<float> (startBeat * ppb);
            const float width = std::max (1.0f, static_cast<float> ((endBeat - startBeat) * ppb));
            const float y = static_cast<float> (firstTrack * trackHeight);
            const float height = static_cast<float> ((lastTrack - firstTrack + 1) * trackHeight);
            const bool selecting = mode == Mode::RangeSelect;
            const float pulse = selecting
                ? 0.45f + 0.2f * std::sin (static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.006))
                : 0.45f;
            const auto selectionBounds = juce::Rectangle<float> (x, y, width, height);
            g.setColour (theme::accent().withAlpha (selecting ? 0.14f : 0.09f));
            g.fillRect (selectionBounds);
            g.setColour (theme::accent().withAlpha (pulse));
            g.drawRect (selectionBounds, selecting ? 2.0f : 1.0f);
            g.drawVerticalLine (static_cast<int> (x), y, y + height);
            g.drawVerticalLine (static_cast<int> (x + width), y, y + height);
            if (! selecting)
            {
                const auto hint = juce::Rectangle<int> (static_cast<int> (x) + 6, static_cast<int> (y) + 5, 245, 18);
                g.setColour (theme::panel().withAlpha (0.9f));
                g.fillRoundedRectangle (hint.toFloat(), 3.0f);
                g.setColour (theme::text());
                g.setFont (juce::FontOptions (10.0f));
                g.drawText ("Ctrl+D duplicar | Delete apagar | Ctrl+J consolidar",
                            hint.reduced (4, 0), juce::Justification::centredLeft, true);
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

            const bool masterTarget = dragTrackIndex == static_cast<int> (project.tracks.size());
            const juce::String message = dragDropAllowed
                ? (masterTarget ? "Solte efeito para adicionar ao Master"
                                : dragDropIsAudio ? "Solte para adicionar audio" : "Solte para adicionar plugin")
                : (masterTarget ? "Master aceita apenas efeitos"
                                : dragDropIsAudio ? "Audio indisponivel nesta trilha" : "Plugin indisponivel nesta trilha");
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
        const auto description = details.description.toString();
        return description.startsWith ("plugin:") || description.startsWith ("audio:");
    }

    void itemDragEnter (const SourceDetails& details) override { updateDragTarget (details); }
    void itemDragMove (const SourceDetails& details) override  { updateDragTarget (details); }
    void itemDragExit (const SourceDetails&) override
    {
        dragTrackIndex = -1;
        dragDropAllowed = false;
        dragDropIsAudio = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void itemDropped (const SourceDetails& details) override
    {
        const auto description = details.description.toString();
        const auto trackIndex = trackIndexAt (details.localPosition.y);
        dragTrackIndex = -1;
        dragDropAllowed = false;
        const bool audioDrop = description.startsWith ("audio:");
        dragDropIsAudio = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
        const auto trackId = trackIdAt (trackIndex);
        const auto beat = owner.snapBeat (details.localPosition.x / owner.pixelsPerBeat);
        if (audioDrop)
        {
            const juce::File file (description.substring (6));
            if (owner.canAcceptAudioDrop && owner.canAcceptAudioDrop (file, trackId) && owner.onAudioDropped)
                owner.onAudioDropped (file, trackId, beat);
            return;
        }

        if (description.startsWith ("plugin:") && owner.canAcceptPluginDrop
            && owner.canAcceptPluginDrop (description.substring (7), trackId) && owner.onPluginDropped)
            owner.onPluginDropped (description.substring (7), trackId, beat);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && isAudioFile (juce::File (files[0]));
    }

    void fileDragEnter (const juce::StringArray& files, int x, int y) override { updateFileDragTarget (files, x, y); }
    void fileDragMove  (const juce::StringArray& files, int x, int y) override { updateFileDragTarget (files, x, y); }

    void fileDragExit (const juce::StringArray&) override
    {
        dragTrackIndex = -1;
        dragDropAllowed = false;
        dragDropIsAudio = false;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        const int trackIndex = trackIndexAt (y);
        if (files.size() != 1)
        {
            dragTrackIndex = -1;
            dragDropAllowed = false;
            dragDropIsAudio = false;
            repaint();
            return;
        }
        const juce::File file (files[0]);
        const int trackId = trackIdAt (trackIndex);
        if (owner.canAcceptAudioDrop && owner.canAcceptAudioDrop (file, trackId) && owner.onAudioDropped)
            owner.onAudioDropped (file, trackId, owner.snapBeat (static_cast<double> (x) / owner.pixelsPerBeat));
        dragTrackIndex = -1;
        dragDropAllowed = false;
        dragDropIsAudio = false;
        repaint();
    }

    // ---- mouse ----
    struct Hit
    {
        int trackIndex = -1;
        roll::Placement* clip = nullptr;
        roll::AudioPlacement* audioClip = nullptr;
    };

    Hit hit (juce::Point<float> p) const
    {
        Hit h;
        h.trackIndex = static_cast<int> (std::floor (p.y / trackHeight));
        if (h.trackIndex < 0 || h.trackIndex >= static_cast<int> (project.tracks.size())) { h.trackIndex = -1; return h; }
        const auto* track = project.tracks[static_cast<size_t> (h.trackIndex)].get();
        const double beat = p.x / owner.pixelsPerBeat;
        if (track->type == roll::TrackType::Audio)
        {
            for (const auto& clip : track->audioClips)
                if (beat >= clip->start && beat < clip->end()) { h.audioClip = clip.get(); break; }
        }
        else
        {
            for (const auto& clip : track->clips)
                if (beat >= clip->start && beat < clip->end()) { h.clip = clip.get(); break; }
        }
        return h;
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        mode = Mode::None; copied = false;

        const int row = static_cast<int> (std::floor (e.position.y / trackHeight));
        if (e.mods.isShiftDown() && e.mods.isLeftButtonDown()
            && row >= 0 && row < static_cast<int> (project.tracks.size()))
        {
            mode = Mode::RangeSelect;
            rangeStartBeat = rangeEndBeat = owner.snapBeat (
                std::max (0.0, e.position.x / owner.pixelsPerBeat));
            rangeStartTrack = rangeEndTrack = row;
            hasRangeSelection = true;
            const int trackId = project.tracks[static_cast<std::size_t> (row)]->id;
            project.selectClip (trackId, -1);
            repaint();
            return;
        }
        hasRangeSelection = false;

        if (row == static_cast<int> (project.tracks.size()))
        {
            project.selectTrack (0);
            return;
        }

        const auto h = hit (e.position);
        if (h.trackIndex < 0) return;
        const int trackId = project.tracks[static_cast<size_t> (h.trackIndex)]->id;

        if (e.mods.isPopupMenu())
        {
            if (h.audioClip != nullptr)
                project.selectAudioClip (trackId, h.audioClip->id);
            else if (h.clip != nullptr)
                project.selectClip (trackId, h.clip->id);
            else
                project.selectClip (trackId, -1);

            showContextMenu (h, trackId);
            return;
        }

        if (h.clip == nullptr && h.audioClip == nullptr)
        {
            project.selectClip (trackId, -1);
            return;
        }

        if (h.audioClip != nullptr)
        {
            project.selectAudioClip (trackId, h.audioClip->id);
            if (owner.onSeek)
                owner.onSeek (std::clamp (static_cast<double> (e.position.x) / owner.pixelsPerBeat,
                                          h.audioClip->start, h.audioClip->end()));
            curTrack = trackId; curClip = -1; curAudioClipId = h.audioClip->id;
            origStart = h.audioClip->start; origLen = h.audioClip->lengthBeats;
            origFadeIn = h.audioClip->fadeInBeats; origFadeOut = h.audioClip->fadeOutBeats;
        }
        else
        {
            project.selectClip (trackId, h.clip->id);
            if (owner.onSeek)
                owner.onSeek (std::clamp (static_cast<double> (e.position.x) / owner.pixelsPerBeat,
                                          h.clip->start, h.clip->end()));
            curTrack = trackId; curClip = h.clip->id; curAudioClipId = -1;
            origStart = h.clip->start; origLen = h.clip->clip.length();
        }
        downBeat = e.position.x / owner.pixelsPerBeat;
        const float right = static_cast<float> ((h.audioClip != nullptr ? h.audioClip->end() : h.clip->end()) * owner.pixelsPerBeat);
        mode = fadeModeAt (h, e.position);
        if (mode == Mode::None)
            mode = (e.position.x > right - 7.0f) ? Mode::Resize : Mode::Move;
        setMouseCursor (mode == Mode::Resize || mode == Mode::FadeIn || mode == Mode::FadeOut
                            ? juce::MouseCursor::LeftRightResizeCursor
                            : juce::MouseCursor::DraggingHandCursor);
    }

    void showContextMenu (const Hit& h, int trackId)
    {
        juce::PopupMenu menu, gridMenu;
        const bool isAudio = h.audioClip != nullptr;
        const int clipId = isAudio ? h.audioClip->id : (h.clip != nullptr ? h.clip->id : -1);

        if (clipId >= 0)
        {
            menu.addItem (1, theme::ui ("Loopar clipe"));
            menu.addItem (2, theme::ui ("Apagar clipe"));
            const auto* selectedTrack = project.track (trackId);
            const auto* audioClip = isAudio && selectedTrack != nullptr
                ? selectedTrack->findAudioClip (clipId) : nullptr;
            const auto* midiClip = ! isAudio && selectedTrack != nullptr
                ? selectedTrack->find (clipId) : nullptr;
            const double clipStart = isAudio ? (audioClip != nullptr ? audioClip->start : 0.0)
                                             : (midiClip != nullptr ? midiClip->start : 0.0);
            const double clipEnd = isAudio ? (audioClip != nullptr ? audioClip->end() : 0.0)
                                           : (midiClip != nullptr ? midiClip->end() : 0.0);
            const bool canSplit = owner.playhead > clipStart + 1.0e-9
                               && owner.playhead < clipEnd - 1.0e-9;
            menu.addItem (3, theme::ui ("Dividir no marcador (Ctrl+E)"), canSplit);
            menu.addSeparator();
        }

        static constexpr double gridValues[] { 1.0, 0.5, 0.25, 0.125, 4.0 };
        static constexpr const char* gridLabels[] { "1/4", "1/8", "1/16", "1/32", "1 compasso" };
        for (int i = 0; i < 5; ++i)
            gridMenu.addItem (100 + i, theme::ui (gridLabels[i]), true,
                              std::abs (owner.gridBeats() - gridValues[i]) < 1.0e-9);
        gridMenu.addItem (105, theme::ui ("Sem grade"), true, owner.gridBeats() <= 0.0);
        menu.addSubMenu (theme::ui ("Grade de edição"), gridMenu);

        juce::Component::SafePointer<Lanes> safeThis (this);
        const double splitBeat = owner.playhead;
        menu.showMenuAsync ({}, [safeThis, trackId, clipId, isAudio, splitBeat] (int result)
        {
            if (safeThis == nullptr) return;

            if (result >= 100 && result <= 104)
            {
                static constexpr double values[] { 1.0, 0.5, 0.25, 0.125, 4.0 };
                safeThis->owner.setGridBeat (values[result - 100]);
                return;
            }
            if (result == 105)
            {
                safeThis->owner.setGridBeat (0.0);
                return;
            }

            if (clipId < 0) return;
            if (result == 1)
            {
                double start = 0.0, end = 0.0;
                if (const auto* track = safeThis->project.track (trackId))
                {
                    if (isAudio)
                    {
                        if (const auto* clip = track->findAudioClip (clipId))
                        {
                            start = clip->start;
                            end = clip->end();
                        }
                    }
                    else if (const auto* clip = track->find (clipId))
                    {
                        start = clip->start;
                        end = clip->end();
                    }
                }

                if (end > start)
                {
                    safeThis->project.setLoop (start, end);
                    if (safeThis->owner.onLoopToggleRequested)
                        safeThis->owner.onLoopToggleRequested (true);
                }
            }
            else if (result == 2)
            {
                if (isAudio)
                    safeThis->project.deleteAudioClip (trackId, clipId);
                else
                    safeThis->project.deleteClip (trackId, clipId);
            }
            else if (result == 3)
            {
                if (isAudio)
                    safeThis->project.splitAudioClip (trackId, clipId, splitBeat);
                else
                    safeThis->project.splitClip (trackId, clipId, splitBeat);
            }
        });
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
        if (mode != Mode::RangeSelect)
            project.beginEdit();
        const double beat = e.position.x / owner.pixelsPerBeat;
        const bool noSnap = e.mods.isAltDown();

        if (mode == Mode::RangeSelect)
        {
            rangeEndBeat = std::max (0.0, noSnap ? beat : owner.snapBeat (beat));
            rangeEndTrack = std::clamp (static_cast<int> (std::floor (e.position.y / trackHeight)),
                                        0, static_cast<int> (project.tracks.size()) - 1);
            repaint();
            return;
        }

        if (mode == Mode::Move)
        {
            double newStart = origStart + (beat - downBeat);
            newStart = std::max (0.0, noSnap ? newStart
                                              : curAudioClipId >= 0 ? owner.snapBeat (newStart) : std::round (newStart));

            const int ti = std::clamp (static_cast<int> (std::floor (e.position.y / trackHeight)), 0, static_cast<int> (project.tracks.size()) - 1);
            const int toTrack = project.tracks[static_cast<size_t> (ti)]->id;

            if (curAudioClipId >= 0)
            {
                if (auto* track = project.track (curTrack))
                    if (auto* cur = track->findAudioClip (curAudioClipId);
                        cur != nullptr && (toTrack != curTrack || std::abs (newStart - cur->start) > 1.0e-9))
                    {
                        const int newId = e.mods.isCtrlDown() && ! copied
                            ? project.copyAudioClip (curTrack, curAudioClipId, toTrack, newStart)
                            : project.moveAudioClip (curTrack, curAudioClipId, toTrack, newStart);
                        if (newId >= 0)
                        {
                            curTrack = toTrack;
                            curAudioClipId = newId;
                            copied = copied || e.mods.isCtrlDown();
                        }
                    }
            }
            else if (auto* track = project.track (curTrack))
            {
                if (auto* cur = track->find (curClip);
                    cur != nullptr && (toTrack != curTrack || std::abs (newStart - cur->start) > 1.0e-9))
                {
                    const int nid = e.mods.isCtrlDown() && ! copied
                        ? project.copyClip (curTrack, curClip, toTrack, newStart)
                        : project.moveClip (curTrack, curClip, toTrack, newStart);
                    if (nid >= 0)
                    {
                        curTrack = toTrack; curClip = nid;
                        copied = copied || e.mods.isCtrlDown();
                    }
                }
            }
        }
        else if (mode == Mode::Resize)
        {
            if (curAudioClipId >= 0)
            {
                const double newLen = std::max (owner.gridBeats(), noSnap ? origLen + (beat - downBeat)
                                                                          : owner.snapBeat (origLen + (beat - downBeat)));
                if (auto* track = project.track (curTrack))
                    if (auto* clip = track->findAudioClip (curAudioClipId);
                        clip != nullptr && std::abs (newLen - clip->lengthBeats) > 1.0e-9)
                        project.resizeAudioClip (curTrack, curAudioClipId, newLen);
            }
            else if (auto* t = project.track (curTrack))
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
                    }
                }
        }
        else if (mode == Mode::FadeIn || mode == Mode::FadeOut)
        {
            if (auto* track = project.track (curTrack))
                if (auto* clip = track->findAudioClip (curAudioClipId))
                {
                    const double delta = beat - downBeat;
                    const double requested = mode == Mode::FadeIn ? origFadeIn + delta
                                                                  : origFadeOut - delta;
                    const double snapped = noSnap ? requested : owner.snapBeat (requested);
                    const double fadeLength = std::clamp (snapped, 0.0, clip->lengthBeats);
                    const double fadeIn = mode == Mode::FadeIn ? fadeLength : clip->fadeInBeats;
                    const double fadeOut = mode == Mode::FadeOut ? fadeLength : clip->fadeOutBeats;
                    project.setAudioClipFades (curTrack, curAudioClipId, fadeIn, fadeOut);
                }
        }

        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        project.endEdit();
        mode = Mode::None;
        updateCursor (e.position);
        curAudioClipId = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto h = hit (e.position);
        if (h.audioClip != nullptr)
        {
            const int trackId = project.tracks[static_cast<size_t> (h.trackIndex)]->id;
            project.selectAudioClip (trackId, h.audioClip->id);
            if (owner.onAudioClipDoubleClicked)
                owner.onAudioClipDoubleClicked (trackId, h.audioClip->id);
            return;
        }
        if (h.trackIndex < 0 || h.clip != nullptr) return;
        const int trackId = project.tracks[static_cast<size_t> (h.trackIndex)]->id;
        const double start = std::floor ((e.position.x / owner.pixelsPerBeat) / 4.0) * 4.0;
        if (project.tracks[static_cast<size_t> (h.trackIndex)]->type == roll::TrackType::Midi)
            project.addClip (trackId, start, 4.0);                     // 1 compasso
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (e.mods.isCtrlDown()) { owner.zoom (w.deltaY > 0 ? 1.15 : 1.0 / 1.15); return; }
        Component::mouseWheelMove (e, w);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();
        const auto key = juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode()));
        auto* t = project.selectedTrack();

        if (k.isKeyCode (juce::KeyPress::deleteKey) || k.isKeyCode (juce::KeyPress::backspaceKey))
        {
            if (hasRangeSelection)
            {
                editRange (false);
                return true;
            }
            if (t != nullptr && project.selectedAudioClipId >= 0)
            {
                project.deleteAudioClip (t->id, project.selectedAudioClipId);
                return true;
            }
            if (t != nullptr && t->find (project.selectedClipId) != nullptr) { project.deleteClip (t->id, project.selectedClipId); return true; }
            if (t != nullptr)
            {
                project.removeTrack (t->id);
                return true;
            }
            return false;
        }

        if (ctrl && key == 'D')
        {
            if (hasRangeSelection)
            {
                editRange (true);
                return true;
            }
            if (t != nullptr && project.selectedAudioClipId >= 0)
            {
                project.duplicateAudioClip (t->id, project.selectedAudioClipId);
                return true;
            }
            if (auto* p = project.selectedClip())
            {
                project.duplicateClip (t->id, p->id);
                return true;
            }
            return false;
        }

        if (ctrl && key == 'J')
        {
            if (! hasRangeSelection || owner.onConsolidateRequested == nullptr)
                return false;
            owner.onConsolidateRequested (std::min (rangeStartBeat, rangeEndBeat),
                                          std::max (rangeStartBeat, rangeEndBeat),
                                          std::min (rangeStartTrack, rangeEndTrack),
                                          std::max (rangeStartTrack, rangeEndTrack));
            return true;
        }

        if (ctrl && key == 'Z' && ! k.getModifiers().isShiftDown())
        {
            project.undo();
            return true;
        }
        if (ctrl && (key == 'Y' || (key == 'Z' && k.getModifiers().isShiftDown())))
        {
            project.redo();
            return true;
        }

        if (ctrl && key == 'L')
        {
            if (! hasRangeSelection)
                return false;

            const double start = std::min (rangeStartBeat, rangeEndBeat);
            const double end = std::max (rangeStartBeat, rangeEndBeat);
            if (end <= start)
                return true;

            project.setLoop (start, end);
            if (owner.onLoopToggleRequested)
                owner.onLoopToggleRequested (true);
            return true;
        }

        if (ctrl && key == 'E')
        {
            if (t != nullptr && project.selectedAudioClipId >= 0)
                project.splitAudioClip (t->id, project.selectedAudioClipId, owner.playhead);
            else if (t != nullptr && project.selectedClipId >= 0)
                project.splitClip (t->id, project.selectedClipId, owner.playhead);
            return true;
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
    void editRange (bool duplicate)
    {
        const double start = std::min (rangeStartBeat, rangeEndBeat);
        const double end = std::max (rangeStartBeat, rangeEndBeat);
        const double length = end - start;
        if (length <= 1.0e-9)
            return;

        const int first = std::min (rangeStartTrack, rangeEndTrack);
        const int last = std::max (rangeStartTrack, rangeEndTrack);
        struct ClipAction { int trackId; int clipId; double start; bool audio; };
        std::vector<ClipAction> actions;
        for (int row = first; row <= last && row < static_cast<int> (project.tracks.size()); ++row)
        {
            const auto* track = project.tracks[static_cast<std::size_t> (row)].get();
            if (track->type == roll::TrackType::Audio)
            {
                for (const auto& clip : track->audioClips)
                    if (clip->start >= start - 1.0e-9 && clip->end() <= end + 1.0e-9
                        && (! duplicate || track->isAudioFree (clip->start + length, clip->lengthBeats)))
                        actions.push_back ({ track->id, clip->id, clip->start, true });
            }
            else
            {
                for (const auto& clip : track->clips)
                    if (clip->start >= start - 1.0e-9 && clip->end() <= end + 1.0e-9
                        && (! duplicate || track->isFree (clip->start + length, clip->clip.length())))
                        actions.push_back ({ track->id, clip->id, clip->start, false });
            }
        }
        if (actions.empty())
            return;

        project.beginEdit();
        for (const auto& action : actions)
        {
            if (action.audio)
            {
                if (duplicate)
                    project.copyAudioClip (action.trackId, action.clipId, action.trackId, action.start + length);
                else
                    project.deleteAudioClip (action.trackId, action.clipId);
            }
            else if (duplicate)
                project.copyClip (action.trackId, action.clipId, action.trackId, action.start + length);
            else
                project.deleteClip (action.trackId, action.clipId);
        }
        project.endEdit();
        repaint();
    }

    int trackIndexAt (int y) const
    {
        const int masterRow = static_cast<int> (project.tracks.size());
        if (y < 0 || y >= (masterRow + 1) * trackHeight)
            return -1;
        return std::min (y / trackHeight, masterRow);
    }

    int trackIdAt (int row) const
    {
        if (row == static_cast<int> (project.tracks.size()))
            return 0;
        if (row >= 0 && row < static_cast<int> (project.tracks.size()))
            return project.tracks[static_cast<std::size_t> (row)]->id;
        return -1;
    }

    void updateDragTarget (const SourceDetails& details)
    {
        const auto description = details.description.toString();
        const int target = trackIndexAt (details.localPosition.y);
        dragDropIsAudio = description.startsWith ("audio:");
        dragTrackIndex = target;
        dragBeat = owner.snapBeat (details.localPosition.x / owner.pixelsPerBeat);
        dragDropAllowed = false;
        const int trackId = trackIdAt (target);
        if (dragDropIsAudio)
            dragDropAllowed = owner.canAcceptAudioDrop
                           && owner.canAcceptAudioDrop (juce::File (description.substring (6)), trackId);
        else if (description.startsWith ("plugin:"))
            dragDropAllowed = owner.canAcceptPluginDrop
                           && owner.canAcceptPluginDrop (description.substring (7), trackId);
        setMouseCursor (dragDropAllowed ? juce::MouseCursor::CopyingCursor
                                        : juce::MouseCursor::NormalCursor);
        repaint();
    }

    static bool isAudioFile (const juce::File& file)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        return extension == ".wav" || extension == ".wave" || extension == ".aif"
            || extension == ".aiff" || extension == ".flac" || extension == ".ogg";
    }

    void updateFileDragTarget (const juce::StringArray& files, int x, int y)
    {
        dragDropIsAudio = true;
        dragTrackIndex = trackIndexAt (y);
        dragBeat = owner.snapBeat (static_cast<double> (x) / owner.pixelsPerBeat);
        dragDropAllowed = false;
        if (files.size() == 1 && owner.canAcceptAudioDrop)
        {
            const int trackId = trackIdAt (dragTrackIndex);
            dragDropAllowed = owner.canAcceptAudioDrop (juce::File (files[0]), trackId);
        }
        setMouseCursor (dragDropAllowed ? juce::MouseCursor::CopyingCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void updateCursor (juce::Point<float> position)
    {
        if (mode == Mode::Resize)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }

        if (mode == Mode::FadeIn || mode == Mode::FadeOut)
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
        if (fadeModeAt (h, position) != Mode::None)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        if (h.clip == nullptr && h.audioClip == nullptr)
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            return;
        }

        const float right = static_cast<float> ((h.audioClip != nullptr ? h.audioClip->end() : h.clip->end()) * owner.pixelsPerBeat);
        setMouseCursor (position.x > right - 7.0f ? juce::MouseCursor::LeftRightResizeCursor
                                                   : juce::MouseCursor::DraggingHandCursor);
    }

    Mode fadeModeAt (const Hit& h, juce::Point<float> position) const
    {
        if (h.audioClip == nullptr)
            return Mode::None;

        const float rowTop = static_cast<float> (h.trackIndex * trackHeight + 22);
        const float left = static_cast<float> (h.audioClip->start * owner.pixelsPerBeat);
        const float right = static_cast<float> (h.audioClip->end() * owner.pixelsPerBeat);
        const float inset = std::min (3.0f, (right - left) * 0.5f);
        const float fadeInX = std::clamp (left + static_cast<float> (h.audioClip->fadeInBeats
                                                                     * owner.pixelsPerBeat),
                                          left + inset, right - inset);
        const float fadeOutX = std::clamp (right - static_cast<float> (h.audioClip->fadeOutBeats
                                                                       * owner.pixelsPerBeat),
                                           left + inset, right - inset);
        const float inDistance = std::hypot (position.x - fadeInX, position.y - rowTop);
        const float outDistance = std::hypot (position.x - fadeOutX, position.y - rowTop);
        constexpr float handleRadius = 10.0f;
        if (inDistance > handleRadius && outDistance > handleRadius)
            return Mode::None;
        return inDistance <= outDistance ? Mode::FadeIn : Mode::FadeOut;
    }

    void drawActionHint (juce::Graphics& g)
    {
        if (mode == Mode::None)
            return;

        const auto* track = project.track (curTrack);
        double clipStart = 0.0, clipLength = 0.0;
        if (curAudioClipId >= 0)
        {
            const auto* clip = track != nullptr ? track->findAudioClip (curAudioClipId) : nullptr;
            if (clip == nullptr) return;
            clipStart = clip->start;
            clipLength = clip->lengthBeats;
        }
        else
        {
            const auto* clip = track != nullptr ? track->find (curClip) : nullptr;
            if (clip == nullptr) return;
            clipStart = clip->start;
            clipLength = clip->clip.length();
        }

        juce::String message;
        if (mode == Mode::Resize)
            message = "Tamanho: " + juce::String (clipLength / 4.0, 1) + " compassos";
        else if (mode == Mode::FadeIn || mode == Mode::FadeOut)
        {
            const auto* clip = track->findAudioClip (curAudioClipId);
            if (clip == nullptr) return;
            const double fadeLength = mode == Mode::FadeIn ? clip->fadeInBeats : clip->fadeOutBeats;
            message = (mode == Mode::FadeIn ? "Fade-in: " : "Fade-out: ")
                    + juce::String (fadeLength, 2) + " beats";
        }
        else
            message = "Movendo: compasso " + juce::String (static_cast<int> (std::floor (clipStart / 4.0)) + 1);
        const int x = std::clamp (static_cast<int> ((clipStart + clipLength) * owner.pixelsPerBeat) + 8, 4, std::max (4, getWidth() - 234));
        const int y = static_cast<int> (project.trackIndex (curTrack)) * trackHeight + 8;
        const auto hint = juce::Rectangle<int> (x, y, 230, 24);
        g.setColour (theme::panel().withAlpha (0.96f));
        g.fillRoundedRectangle (hint.toFloat(), 5.0f);
        g.setColour (theme::accent());
        g.drawRoundedRectangle (hint.toFloat(), 5.0f, 1.0f);
        g.setColour (theme::text());
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (message, hint.reduced (7, 0), juce::Justification::centredLeft, true);
    }

    ProjectModel& project;
    ArrangementView& owner;
    Mode mode = Mode::None;
    bool copied = false;
    bool dragDropAllowed = false, dragDropIsAudio = false;
    int curTrack = -1, curClip = -1, curAudioClipId = -1;
    int dragTrackIndex = -1;
    double origStart = 0.0, origLen = 4.0, downBeat = 0.0, dragBeat = 0.0;
    double origFadeIn = 0.0, origFadeOut = 0.0;
    bool hasRangeSelection = false;
    double rangeStartBeat = 0.0, rangeEndBeat = 0.0;
    int rangeStartTrack = 0, rangeEndTrack = 0;
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
        const double ppb = owner.pixelsPerBeat;

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
            const double ppb = owner.pixelsPerBeat;
            const float lx0 = static_cast<float> (project.loopStart * ppb), lx1 = static_cast<float> (project.loopEnd * ppb);
            if (std::abs (e.position.x - lx0) < 6.0f)       mode = Mode::LoopLeft;
            else if (std::abs (e.position.x - lx1) < 6.0f)  mode = Mode::LoopRight;
            else if (e.position.x > lx0 && e.position.x < lx1) { mode = Mode::LoopMove; grabOffset = e.position.x / ppb - project.loopStart; }
        }
        hoverMode = mode;
        updateCursor (e.position);
        if (mode == Mode::Seek && owner.onSeek) owner.onSeek (std::max (0.0, e.position.x / owner.pixelsPerBeat));
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
        const double beat = std::max (0.0, std::round (e.position.x / owner.pixelsPerBeat));
        switch (mode)
        {
            case Mode::Seek:      if (owner.onSeek) owner.onSeek (std::max (0.0, e.position.x / owner.pixelsPerBeat)); break;
            case Mode::LoopLeft:  project.setLoop (std::min (beat, project.loopEnd - 1.0), project.loopEnd); break;
            case Mode::LoopRight: project.setLoop (project.loopStart, std::max (beat, project.loopStart + 1.0)); break;
            case Mode::LoopMove:
            {
                const double len = project.loopEnd - project.loopStart;
                const double s = std::max (0.0, std::round (e.position.x / owner.pixelsPerBeat - grabOffset));
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

        const float left = static_cast<float> (project.loopStart * owner.pixelsPerBeat);
        const float right = static_cast<float> (project.loopEnd * owner.pixelsPerBeat);
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
            message = "Inicio do loop: " + formatPosition (project.loopStart);
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
        addAndMakeVisible (volumeDb);
        addAndMakeVisible (routingButton);

        routingButton.setMouseClickGrabsKeyboardFocus (false);
        updateRoutingButton();
        routingButton.onClick = [this]
        {
            if (auto* tr = project.track (trackId))
            {
                auto next = (tr->outputRouting == roll::TrackOutputRouting::Master)
                                ? roll::TrackOutputRouting::ExtOut
                                : roll::TrackOutputRouting::Master;
                project.setTrackRouting (trackId, next);
                updateRoutingButton();
            }
        };

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
        volume.setPopupDisplayEnabled (true, true, nullptr);
        volume.textFromValueFunction = [] (double v)
        {
            const float db = roll::gainToDecibels (static_cast<float> (v));
            if (db <= -59.5f) return juce::String ("-inf dB");
            return (db >= 0.05f ? juce::String ("+") : juce::String()) + juce::String (db, 1) + " dB";
        };
        volumeDb.setFont (juce::FontOptions (10.0f));
        volumeDb.setColour (juce::Label::textColourId, theme::textDim());
        volumeDb.setJustificationType (juce::Justification::centredRight);
        updateVolumeText (t->volume);
        volume.onValueChange = [this]
        {
            if (auto* tr = project.track (trackId))
            {
                tr->volume = volume.getValue();
                updateVolumeText (tr->volume);
                project.notifyMixer();
            }
        };
        volume.setDoubleClickReturnValue (true, 1.0);
    }

    int id() const { return trackId; }
    void setPeak (float p) { if (std::abs (p - peak) > 0.004f) { peak = p; repaint (getWidth() - 16, 0, 16, getHeight()); } }
    void refreshText()
    {
        updateRoutingButton();
        if (const auto* tr = project.track (trackId))
        {
            volume.setValue (tr->volume, juce::dontSendNotification);
            updateVolumeText (tr->volume);
        }
        repaint();
    }

    void updateVolumeText (double gain)
    {
        const float db = roll::gainToDecibels (static_cast<float> (gain));
        volumeDb.setText (db <= -59.5f ? "-inf dB"
                                       : (db >= 0.05f ? "+" : "") + juce::String (db, 1) + " dB",
                          juce::dontSendNotification);
    }

    void updateRoutingButton()
    {
        if (auto* tr = project.track (trackId))
        {
            const bool ext = (tr->outputRouting == roll::TrackOutputRouting::ExtOut);
            routingButton.setButtonText (ext ? "Ext. Out" : "Master");
            routingButton.setColour (juce::TextButton::buttonColourId, ext ? juce::Colour (0xffe67e22) : theme::panelLight());
            routingButton.setColour (juce::TextButton::textColourOffId, ext ? juce::Colours::black : theme::textDim());
            routingButton.setTooltip (ext ? theme::ui ("Destino: Ext. Out (Direto - bypassa o Master)")
                                          : theme::ui ("Destino: Master (processado pelo canal Master)"));
        }
    }

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
        auto topRow = a.removeFromTop (22);
        routingButton.setBounds (topRow.removeFromRight (58));
        topRow.removeFromRight (4);
        name.setBounds (topRow);
        a.removeFromTop (18);
        auto row = a.removeFromTop (24);
        mute.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (4);
        solo.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (6);
        volumeIconBounds = row.removeFromLeft (20);
        const int dbWidth = std::min (48, row.getWidth() / 3);
        volumeDb.setBounds (row.removeFromRight (dbWidth));
        row.removeFromRight (4);
        volume.setBounds (row);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        project.selectClip (trackId, -1);
        owner.grabFocus();
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, theme::ui ("Nova trilha MIDI"));
            m.addItem (3, theme::ui ("Nova trilha de audio"));
            m.addItem (2, theme::ui ("Apagar trilha"), project.tracks.size() > 1);
            const int tid = trackId;
            auto* proj = &project;
            m.showMenuAsync ({}, [proj, tid] (int r)
            {
                if (r == 1) proj->addTrack();
                if (r == 2) proj->removeTrack (tid);
                if (r == 3) proj->addTrack (roll::TrackType::Audio);
            });
        }
    }

private:
    ProjectModel& project;
    ArrangementView& owner;
    int trackId;
    juce::Rectangle<int> volumeIconBounds;
    juce::Label name, volumeDb;
    juce::TextButton mute { "M" }, solo { "S" };
    juce::TextButton routingButton;
    juce::Slider volume;
    float peak = 0.0f;
};

// ============================================================ MasterHeader (trilha Master)

class ArrangementView::MasterHeader : public juce::Component, public juce::DragAndDropTarget
{
public:
    MasterHeader (ProjectModel& p, ArrangementView& o) : project (p), owner (o)
    {
        addAndMakeVisible (name);
        addAndMakeVisible (volume);
        addAndMakeVisible (dbLabel);

        name.setText ("Master", juce::dontSendNotification);
        name.setTooltip (theme::ui ("Clique para selecionar. Arraste um efeito para adiciona-lo ao Master."));
        name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        name.setColour (juce::Label::textColourId, theme::text());

        volume.setSliderStyle (juce::Slider::LinearHorizontal);
        volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        volume.setRange (-60.0, 6.0, 0.1);
        volume.setValue (0.0, juce::dontSendNotification);
        volume.setDoubleClickReturnValue (true, 0.0);
        volume.setTooltip (theme::ui ("Volume Master (-inf a +6.0 dB)"));
        volume.setMouseClickGrabsKeyboardFocus (false);

        dbLabel.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        dbLabel.setColour (juce::Label::textColourId, theme::textDim());
        dbLabel.setJustificationType (juce::Justification::centredRight);
        updateDbText (0.0f);

        volume.onValueChange = [this]
        {
            const float db = static_cast<float> (volume.getValue());
            updateDbText (db);
            if (project.masterTrack != nullptr)
            {
                project.masterTrack->volume = roll::decibelsToGain (db);
                project.notifyMixer();
            }
        };
    }

    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        const auto description = details.description.toString();
        return description.startsWith ("plugin:")
            && owner.canAcceptPluginDrop
            && owner.canAcceptPluginDrop (description.substring (7), 0);
    }

    void itemDragEnter (const SourceDetails&) override
    {
        pluginDragOver = true;
        repaint();
    }

    void itemDragMove (const SourceDetails&) override {}

    void itemDragExit (const SourceDetails&) override
    {
        pluginDragOver = false;
        repaint();
    }

    void itemDropped (const SourceDetails& details) override
    {
        pluginDragOver = false;
        repaint();

        const auto description = details.description.toString();
        if (description.startsWith ("plugin:") && owner.onPluginDropped)
            owner.onPluginDropped (description.substring (7), 0, 0.0);
    }

    void updateDbText (float db)
    {
        if (db <= -59.5f)
            dbLabel.setText ("-inf dB", juce::dontSendNotification);
        else
        {
            juce::String s = (db >= 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
            dbLabel.setText (s, juce::dontSendNotification);
        }
    }

    void setPeak (float p)
    {
        if (p >= 1.0f)
            clipHeld = true;

        if (std::abs (p - peak) > 0.003f)
        {
            peak = p;
            repaint (getWidth() - 24, 0, 24, getHeight());
        }
    }

    void refreshText()
    {
        if (project.masterTrack != nullptr)
        {
            const float db = roll::gainToDecibels (static_cast<float> (project.masterTrack->volume));
            volume.setValue (db, juce::dontSendNotification);
            updateDbText (db);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const bool sel = (project.selectedTrackId == 0);
        g.setColour (sel ? theme::panelLight().brighter (0.15f) : theme::panel());
        g.fillRect (getLocalBounds());

        // Faixa de cor da trilha Master (ouro/laranja)
        g.setColour (juce::Colour (0xffe67e22));
        g.fillRect (0, 0, 6, getHeight());

        g.setColour (theme::border());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

        // Resumo de efeitos na Master
        g.setColour (theme::textDim());
        g.setFont (juce::FontOptions (11.0f));
        const juce::String summary = owner.deviceSummary ? owner.deviceSummary (0) : juce::String();
        g.drawText (summary.isEmpty() ? theme::ui ("sem efeitos") : summary, 14, 26, getWidth() - 40, 16, juce::Justification::centredLeft, true);

        if (const auto* volumeIcon = icons::get (icons::Icon::Volume, theme::textDim(), 16))
            volumeIcon->drawWithin (g, volumeIconBounds.toFloat(), juce::RectanglePlacement::centred, 1.0f);

        // Medidor de decibeis (dBFS) com indicador de clip
        const int meterX = getWidth() - 18;
        const int meterY = 6;
        const int meterW = 10;
        const int meterH = getHeight() - 12;
        const int clipBoxH = 7;
        const int barH = meterH - clipBoxH - 2;
        const int barY = meterY + clipBoxH + 2;

        // Clip indicator box
        g.setColour (clipHeld ? juce::Colour (0xffff3b30) : juce::Colours::black.withAlpha (0.45f));
        g.fillRect (meterX, meterY, meterW, clipBoxH);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawRect (meterX, meterY, meterW, clipBoxH, 1);

        // Fundo do medidor
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (meterX, barY, meterW, barH);

        // Nível em dB (-48 dB até 0 dB)
        const float db = roll::gainToDecibels (peak);
        const float norm = std::clamp ((db + 48.0f) / 48.0f, 0.0f, 1.0f);
        const int fillH = static_cast<int> (norm * barH);

        if (fillH > 0)
        {
            // Cores: verde ate -12dB (norm < 0.75), amarelo/laranja ate 0dB
            const auto barColour = norm > 0.75f ? (norm >= 0.98f ? theme::warning() : juce::Colour (0xfff39c12))
                                                : theme::accent();
            g.setColour (barColour);
            g.fillRect (meterX, barY + barH - fillH, meterW, fillH);
        }

        // Marcas de -12 dB e 0 dB
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawHorizontalLine (barY, static_cast<float> (meterX - 2), static_cast<float> (meterX + meterW));
        g.drawHorizontalLine (barY + static_cast<int> (barH * 0.25f), static_cast<float> (meterX - 2), static_cast<float> (meterX + meterW));
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! pluginDragOver)
            return;

        g.setColour (theme::accent().withAlpha (0.18f));
        g.fillRect (getLocalBounds());
        g.setColour (theme::accent());
        g.drawRect (getLocalBounds(), 2);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (theme::ui ("Solte para adicionar efeito ao Master"),
                    getLocalBounds().reduced (8), juce::Justification::centred, true);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (6, 4);
        a.removeFromLeft (8);
        a.removeFromRight (22);
        auto topRow = a.removeFromTop (22);
        name.setBounds (topRow.removeFromLeft (70));
        dbLabel.setBounds (topRow);
        a.removeFromTop (18);
        auto row = a.removeFromTop (24);
        volumeIconBounds = row.removeFromLeft (20);
        row.removeFromLeft (4);
        volume.setBounds (row);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        project.selectTrack (0);
        owner.grabFocus();

        // Se clicou no clip box, reseta o clip
        if (e.x >= getWidth() - 24 && e.y <= 16)
        {
            clipHeld = false;
            repaint();
        }
    }

private:
    ProjectModel& project;
    ArrangementView& owner;
    juce::Label name, dbLabel;
    juce::Slider volume;
    juce::Rectangle<int> volumeIconBounds;
    float peak = 0.0f;
    bool clipHeld = false, pluginDragOver = false;
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
    addAndMakeVisible (addAudioTrackButton);
    addAndMakeVisible (gridBox);
    rulerHolder.addAndMakeVisible (*ruler);

    viewport.setViewedComponent (lanes.get(), false);
    viewport.setScrollBarsShown (true, true);

    addTrackButton.setMouseClickGrabsKeyboardFocus (false);
    addAudioTrackButton.setMouseClickGrabsKeyboardFocus (false);
    addTrackButton.setTooltip ("Adicionar trilha MIDI (Ctrl+Shift+T)");
    addAudioTrackButton.setTooltip ("Adicionar trilha de audio (Ctrl+Shift+A)");
    gridBox.setMouseClickGrabsKeyboardFocus (false);
    addTrackButton.onClick = [this] { project.addTrack(); };
    addAudioTrackButton.onClick = [this] { project.addTrack (roll::TrackType::Audio); };
    gridBox.addItem ("1/4", 1);
    gridBox.addItem ("1/8", 2);
    gridBox.addItem ("1/16", 3);
    gridBox.addItem ("1/32", 4);
    gridBox.addItem ("1 compasso", 5);
    gridBox.addItem ("Sem grade", 6);
    gridBox.setSelectedId (3, juce::dontSendNotification);
    gridBox.onChange = [this]
    {
        static constexpr double values[] = { 1.0, 0.5, 0.25, 0.125, 4.0 };
        const int id = gridBox.getSelectedId();
        if (id >= 1 && id <= 5)
            setGridBeat (values[id - 1]);
        else if (id == 6)
            setGridBeat (0.0);
    };

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
    pixelsPerBeat = std::clamp (pixelsPerBeat * factor, 4.0, 120.0);
    updateSizes();
    lanes->repaint();
    ruler->repaint();
}

double ArrangementView::snapBeat (double beat) const
{
    if (gridBeat <= 0.0)
        return std::max (0.0, beat);
    return std::max (0.0, std::round (beat / gridBeat) * gridBeat);
}

void ArrangementView::setGridBeat (double beats)
{
    gridBeat = std::max (0.0, beats);
    static constexpr double values[] { 1.0, 0.5, 0.25, 0.125, 4.0 };
    int selectedId = 0;
    for (int i = 0; i < 5; ++i)
        if (std::abs (gridBeat - values[i]) < 1.0e-9)
            selectedId = i + 1;
    if (gridBeat <= 0.0)
        selectedId = 6;
    gridBox.setSelectedId (selectedId, juce::dontSendNotification);
    lanes->repaint();
    if (onGridChanged) onGridChanged();
}

void ArrangementView::setPlayhead (double beats)
{
    const int oldX = static_cast<int> (playhead * pixelsPerBeat);
    lanes->setPlayhead (beats);
    ruler->repaint (oldX - 6, 0, 14, rulerHeight);
    ruler->repaint (static_cast<int> (beats * pixelsPerBeat) - 6, 0, 14, rulerHeight);
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
    if (masterHeader == nullptr)
    {
        masterHeader = std::make_unique<MasterHeader> (project, *this);
        headerHolder.addAndMakeVisible (*masterHeader);
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
    if (masterHeader != nullptr)
        masterHeader->setBounds (0, static_cast<int> (headers.size()) * trackHeight - vp.y, headerWidth, trackHeight);
}

void ArrangementView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (headers.size() != project.tracks.size()) { refresh(); }
    else
    {
        updateSizes();
        for (auto& h : headers) h->refreshText();
        if (masterHeader != nullptr) masterHeader->refreshText();
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
    {
        for (auto& h : headers) h->setPeak (meterProvider (h->id()));
        if (masterHeader != nullptr) masterHeader->setPeak (meterProvider (0));
    }
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
    auto addTrackArea = top.removeFromLeft (headerWidth).reduced (4);
    addTrackButton.setBounds (addTrackArea.removeFromLeft (56));
    addTrackArea.removeFromLeft (3);
    addAudioTrackButton.setBounds (addTrackArea.removeFromLeft (64));
    addTrackArea.removeFromLeft (3);
    gridBox.setBounds (addTrackArea);
    rulerHolder.setBounds (top);
    auto left = area.removeFromLeft (headerWidth);
    viewport.setBounds (area);
    headerHolder.setBounds (left.withHeight (viewport.getMaximumVisibleHeight()));
    updateSizes();
}
