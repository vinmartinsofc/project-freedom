#include "PianoRollEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
bool isBlackKey (int pitch)
{
    const int pc = pitch % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

// Convenção do Live: nota 60 = C3.
juce::String pitchName (int pitch)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[pitch % 12]) + juce::String (pitch / 12 - 2);
}

bool isLetter (const juce::KeyPress& k, char c)
{
    return juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode())) == static_cast<juce::juce_wchar> (c);
}
} // namespace

// ============================================================ NoteGrid

void NoteGrid::updateSize()
{
    setSize (std::max (1, static_cast<int> (model.gridWidthPixels())), std::max (1, model.totalHeight()));
}

void NoteGrid::setPlayhead (double beats)
{
    const float oldX = beatsToX (playheadBeats), newX = beatsToX (beats);
    playheadBeats = beats;
    repaint (static_cast<int> (oldX) - 2, 0, 5, getHeight());
    repaint (static_cast<int> (newX) - 2, 0, 5, getHeight());
}

juce::Rectangle<float> NoteGrid::noteRect (const roll::Note& n) const
{
    const int row = model.pitchToRow (n.pitch);
    if (row < 0) return {};
    return { beatsToX (n.startBeats), static_cast<float> (row * model.rowHeight + 1),
             std::max (3.0f, static_cast<float> (n.lengthBeats * model.pixelsPerBeat)),
             static_cast<float> (model.rowHeight - 2) };
}

int NoteGrid::hitNote (juce::Point<float> p) const
{
    const auto& notes = model.clip().notes();
    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
        if (noteRect (*it).contains (p)) return it->id;
    return -1;
}

void NoteGrid::paint (juce::Graphics& g)
{
    const auto clipBox = g.getClipBounds();
    if (! model.hasClip())
    {
        g.fillAll (theme::rowBlack());
        g.setColour (theme::textDim());
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (theme::ui ("Selecione um clip no arranjo\n(duplo clique em uma trilha cria um clip)"),
                          getLocalBounds().reduced (10), juce::Justification::centred, 3);
        return;
    }
    const int firstRow = std::max (0, clipBox.getY() / model.rowHeight);
    const int lastRow  = std::min (static_cast<int> (model.rows().size()) - 1, clipBox.getBottom() / model.rowHeight);

    for (int r = firstRow; r <= lastRow; ++r)
    {
        const int pitch = model.rowToPitch (r);
        juce::Colour bg = isBlackKey (pitch) ? theme::rowBlack() : theme::rowWhite();
        const bool showScale = model.scaleOn && model.highlight;
        if (showScale && ! model.foldScale && model.scale.contains (pitch))
            bg = theme::rowInScale().withMultipliedBrightness (isBlackKey (pitch) ? 0.85f : 1.0f);
        if (showScale && pitch % 12 == model.scale.root())
            bg = bg.brighter (0.12f);                       // tônica em destaque
        g.setColour (bg);
        g.fillRect (clipBox.getX(), r * model.rowHeight, clipBox.getWidth(), model.rowHeight);

        g.setColour (pitch % 12 == 0 ? theme::border().brighter (0.3f) : theme::border().withAlpha (0.45f));
        g.drawHorizontalLine ((r + 1) * model.rowHeight - 1, static_cast<float> (clipBox.getX()), static_cast<float> (clipBox.getRight()));
    }

    // linhas verticais: grade, beat, compasso
    const double firstBeat = std::floor (xToBeats (static_cast<float> (clipBox.getX())) / model.gridBeats) * model.gridBeats;
    for (double b = std::max (0.0, firstBeat); b <= model.clip().length() + 1e-9; b += model.gridBeats)
    {
        const float x = beatsToX (b);
        if (x > clipBox.getRight()) break;
        const bool bar  = std::abs (b / 4.0 - std::round (b / 4.0)) < 1e-6;
        const bool beat = std::abs (b - std::round (b)) < 1e-6;
        g.setColour (bar ? theme::border().brighter (0.6f) : beat ? theme::border().brighter (0.2f) : theme::border().withAlpha (0.5f));
        g.drawVerticalLine (static_cast<int> (x), static_cast<float> (clipBox.getY()), static_cast<float> (clipBox.getBottom()));
    }

    // notas
    for (const auto& n : model.clip().notes())
    {
        auto r = noteRect (n);
        if (r.isEmpty() || ! r.toNearestInt().intersects (clipBox)) continue;

        const bool sel = model.isSelected (n.id);
        const float vel = static_cast<float> (n.velocity) / 127.0f;
        juce::Colour c = sel ? theme::noteSelected() : theme::noteFill();
        c = c.withMultipliedBrightness (0.55f + 0.45f * vel);
        if (n.muted) c = juce::Colour (0xff5b6169);

        g.setColour (c);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }

    if (drag == Drag::Rubber)
    {
        g.setColour (theme::accent().withAlpha (0.18f));
        g.fillRect (rubberRect);
        g.setColour (theme::accent());
        g.drawRect (rubberRect, 1.0f);
    }

    g.setColour (theme::accent().withAlpha (0.9f));                      // marcador de inserção
    g.drawVerticalLine (static_cast<int> (beatsToX (model.insertBeat)), 0.0f, static_cast<float> (getHeight()));

    g.setColour (theme::playhead());
    g.drawVerticalLine (static_cast<int> (beatsToX (playheadBeats)), 0.0f, static_cast<float> (getHeight()));
}

void NoteGrid::setPreview (int pitch)
{
    if (previewPitch == pitch) return;
    if (previewPitch >= 0 && model.onPreview) model.onPreview (previewPitch, false);
    previewPitch = pitch;
    if (previewPitch >= 0 && model.onPreview) model.onPreview (previewPitch, true);
}

void NoteGrid::flashPreview (int pitch)
{
    if (! model.onPreview) return;
    model.onPreview (pitch, true);
    juce::Component::SafePointer<NoteGrid> safe (this);
    juce::Timer::callAfterDelay (200, [safe, pitch]
    {
        if (safe != nullptr && safe->model.onPreview) safe->model.onPreview (pitch, false);
    });
}

void NoteGrid::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (! model.hasClip()) return;
    const auto p = e.position;
    const int id = hitNote (p);
    drag = Drag::None;
    editBegun = false;
    copied = false;
    original.clear();
    downBeat = xToBeats (p.x);

    if (id >= 0)
    {
        if (e.mods.isShiftDown() && model.isSelected (id))
        {
            model.selection.erase (id);
            model.sendChangeMessage();
            return;
        }
        if (! model.isSelected (id))
        {
            if (! e.mods.isShiftDown()) model.selection.clear();
            model.selection.insert (id);
        }

        primaryId = id;
        for (int sid : model.selection)
            if (auto* n = model.clip().find (sid)) original[sid] = *n;

        const auto r = noteRect (*model.clip().find (id));
        const float edge = std::min (6.0f, r.getWidth() * 0.4f);
        drag = (p.x > r.getRight() - edge) ? Drag::Resize : Drag::Move;
        setPreview (model.clip().find (id)->pitch);
        model.sendChangeMessage();
        return;
    }

    const int pitch = model.rowToPitch (yToRow (p.y));
    if (model.drawMode && pitch >= 0)
    {
        const double start = model.snap ? std::floor (downBeat / model.gridBeats) * model.gridBeats : downBeat;
        model.beginEdit();
        editBegun = true;
        roll::Note n;
        n.pitch = pitch;
        n.startBeats = std::max (0.0, start);
        n.lengthBeats = model.minLength();
        primaryId = model.clip().addNote (n);
        model.selection = { primaryId };
        original[primaryId] = *model.clip().find (primaryId);
        downBeat = n.endBeats();
        drag = Drag::Resize;
        setPreview (pitch);
        model.notify (false);
        return;
    }

    if (! e.mods.isShiftDown())
        model.setInsertMarker (model.snapBeats (downBeat));       // clique no fundo = marcador de inserção

    baseSelection = e.mods.isShiftDown() ? model.selection : std::set<int>{};
    model.selection = baseSelection;
    rubberStart = p;
    rubberRect = {};
    drag = Drag::Rubber;
    model.sendChangeMessage();
}

void NoteGrid::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = e.position;

    if (drag == Drag::Rubber)
    {
        rubberRect = juce::Rectangle<float> (rubberStart, p);
        model.selection = baseSelection;
        for (const auto& n : model.clip().notes())
            if (noteRect (n).intersects (rubberRect)) model.selection.insert (n.id);
        repaint();
        model.sendChangeMessage();
        return;
    }

    if ((drag != Drag::Move && drag != Drag::Resize) || original.empty()) return;
    if (! editBegun)
    {
        if (e.getDistanceFromDragStart() < 3) return;
        model.beginEdit();
        editBegun = true;
    }

    const double beat = xToBeats (p.x);
    const auto& prim = original.at (primaryId);

    if (drag == Drag::Move)
    {
        if (e.mods.isCtrlDown() && ! copied)                       // Ctrl+arrastar: deixa cópias no lugar
        {
            for (const auto& [id, o] : original)
            {
                roll::Note c = o;
                model.clip().addNote (c);
            }
            copied = true;
        }

        const bool noSnap = e.mods.isAltDown();
        double newStart = prim.startBeats + (beat - downBeat);
        newStart = noSnap ? newStart : model.snapBeats (newStart);
        double dBeats = newStart - prim.startBeats;

        const int row = std::clamp (yToRow (p.y), 0, static_cast<int> (model.rows().size()) - 1);
        int dPitch = model.rowToPitch (row) - prim.pitch;

        for (const auto& [id, o] : original)
        {
            dBeats = std::max (dBeats, -o.startBeats);
            dBeats = std::min (dBeats, model.clip().length() - model.minLength() - o.startBeats);
            dPitch = std::clamp (dPitch, -o.pitch, 127 - o.pitch);
        }

        for (const auto& [id, o] : original)
            if (auto* n = model.clip().find (id)) { n->startBeats = o.startBeats + dBeats; n->pitch = o.pitch + dPitch; }

        setPreview (prim.pitch + dPitch);
    }
    else
    {
        const bool noSnap = e.mods.isAltDown();
        double newEnd = prim.endBeats() + (beat - downBeat);
        newEnd = noSnap ? newEnd : model.snapBeats (newEnd);
        double dLen = newEnd - prim.endBeats();

        for (const auto& [id, o] : original)
            dLen = std::max (dLen, model.minLength() - o.lengthBeats);

        for (const auto& [id, o] : original)
            if (auto* n = model.clip().find (id)) n->lengthBeats = o.lengthBeats + dLen;
    }

    model.notify (false);
}

void NoteGrid::mouseUp (const juce::MouseEvent&)
{
    setPreview (-1);
    if (editBegun)
    {
        model.resolveOverlaps (model.selection);
        model.notify (true);
    }
    drag = Drag::None;
    editBegun = false;
    rubberRect = {};
    original.clear();
    repaint();
}

void NoteGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! model.hasClip()) return;
    const auto p = e.position;
    const int id = hitNote (p);

    if (id >= 0)                                   // duplo clique em nota: apaga
    {
        model.beginEdit();
        model.clip().removeNote (id);
        model.selection.erase (id);
        model.notify (true);
        return;
    }

    const int pitch = model.rowToPitch (yToRow (p.y));
    if (pitch < 0) return;

    const double beat = xToBeats (p.x);
    roll::Note n;
    n.pitch = pitch;
    n.startBeats = std::max (0.0, model.snap ? std::floor (beat / model.gridBeats) * model.gridBeats : beat);
    n.lengthBeats = model.snap ? model.gridBeats : 0.25;
    if (n.startBeats >= model.clip().length()) return;

    model.beginEdit();
    const int newId = model.clip().addNote (n);
    model.selection = { newId };
    model.notify (true);
    flashPreview (pitch);
}

void NoteGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown())                       // Ctrl + roda: zoom horizontal
    {
        const double factor = w.deltaY > 0 ? 1.15 : 1.0 / 1.15;
        model.pixelsPerBeat = std::clamp (model.pixelsPerBeat * factor, 12.0, 320.0);
        model.sendChangeMessage();
        return;
    }
    if (e.mods.isAltDown())                        // Alt + roda: zoom vertical
    {
        model.rowHeight = std::clamp (model.rowHeight + (w.deltaY > 0 ? 2 : -2), 8, 32);
        model.sendChangeMessage();
        return;
    }
    Component::mouseWheelMove (e, w);              // repassa ao viewport (rolagem)
}

bool NoteGrid::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const bool ctrl = mods.isCtrlDown() || mods.isCommandDown();
    const bool shift = mods.isShiftDown();
    const int code = k.getKeyCode();

    if (ctrl && isLetter (k, 'A')) { model.selectAll(); return true; }
    if (ctrl && isLetter (k, 'D')) { model.duplicateSelected(); return true; }
    if (ctrl && isLetter (k, 'C')) { model.copySelected (false); return true; }
    if (ctrl && isLetter (k, 'X')) { model.copySelected (true);  return true; }
    if (ctrl && isLetter (k, 'V')) { model.paste(); return true; }
    if (ctrl && isLetter (k, 'U')) { model.quantizeSelected(); return true; }
    if (ctrl && isLetter (k, 'Z') && ! shift) { model.undo(); return true; }
    if (ctrl && (isLetter (k, 'Y') || (isLetter (k, 'Z') && shift))) { model.redo(); return true; }

    if (ctrl && code == '1') { model.narrowGrid();    return true; }
    if (ctrl && code == '2') { model.widenGrid();     return true; }
    if (ctrl && code == '3') { model.toggleTriplet(); return true; }
    if (ctrl && code == '4') { model.toggleSnap();    return true; }

    if (! ctrl && isLetter (k, 'B')) { model.drawMode = ! model.drawMode; model.sendChangeMessage(); return true; }
    if (! ctrl && isLetter (k, 'K')) { model.highlight = ! model.highlight; model.sendChangeMessage(); return true; }
    if (! ctrl && code == '0')       { model.toggleMuteSelected(); return true; }

    if (k.isKeyCode (juce::KeyPress::escapeKey)) { model.selection.clear(); model.sendChangeMessage(); return true; }
    if (k.isKeyCode (juce::KeyPress::homeKey))   { model.setInsertMarker (0.0);                return true; }
    if (k.isKeyCode (juce::KeyPress::endKey))    { model.setInsertMarker (model.clip().length()); return true; }

    // zoom horizontal com + e -
    if (! ctrl)
    {
        const auto ch = k.getTextCharacter();
        if (ch == '+' || ch == '=') { model.pixelsPerBeat = std::min (320.0, model.pixelsPerBeat * 1.25); model.sendChangeMessage(); return true; }
        if (ch == '-' || ch == '_') { model.pixelsPerBeat = std::max (12.0,  model.pixelsPerBeat / 1.25); model.sendChangeMessage(); return true; }
    }

    // Page Up/Down: rola na vertical; Ctrl+Page Up/Down: na horizontal
    if (k.isKeyCode (juce::KeyPress::pageUpKey) || k.isKeyCode (juce::KeyPress::pageDownKey))
    {
        if (auto* vp = findParentComponentOfClass<juce::Viewport>())
        {
            const int dir = k.isKeyCode (juce::KeyPress::pageUpKey) ? -1 : 1;
            auto pos = vp->getViewPosition();
            if (ctrl) pos.x += dir * vp->getMaximumVisibleWidth() * 9 / 10;
            else      pos.y += dir * vp->getMaximumVisibleHeight() * 9 / 10;
            vp->setViewPosition (std::max (0, pos.x), std::max (0, pos.y));
            return true;
        }
    }

    if (k.isKeyCode (juce::KeyPress::deleteKey) || k.isKeyCode (juce::KeyPress::backspaceKey))
    {
        if (model.selection.empty()) return false;
        model.deleteSelected();
        return true;
    }

    if (! model.selection.empty() && ! ctrl)
    {
        if (k.isKeyCode (juce::KeyPress::upKey))   { model.nudgeSelected (0.0, shift ? 12 : 1);  return true; }
        if (k.isKeyCode (juce::KeyPress::downKey)) { model.nudgeSelected (0.0, shift ? -12 : -1); return true; }
        if (k.isKeyCode (juce::KeyPress::leftKey))  { shift ? model.resizeSelected (-model.gridBeats) : model.nudgeSelected (-model.gridBeats, 0); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { shift ? model.resizeSelected ( model.gridBeats) : model.nudgeSelected ( model.gridBeats, 0); return true; }
    }
    return false;                                  // Espaço etc: sobe para a janela principal
}

// ============================================================ KeyColumn

void KeyColumn::paint (juce::Graphics& g)
{
    const auto clipBox = g.getClipBounds();
    const int firstRow = std::max (0, clipBox.getY() / model.rowHeight);
    const int lastRow  = std::min (static_cast<int> (model.rows().size()) - 1, clipBox.getBottom() / model.rowHeight);

    for (int r = firstRow; r <= lastRow; ++r)
    {
        const int pitch = model.rowToPitch (r);
        const int y = r * model.rowHeight;
        const bool black = isBlackKey (pitch);

        const bool lit = model.isKeyLit (pitch);
        g.setColour (lit ? theme::accent() : black ? juce::Colour (0xff15171a) : juce::Colour (0xffdfe3e8));
        g.fillRect (0, y, getWidth(), model.rowHeight - 1);

        if (model.scaleOn && model.highlight && model.scale.contains (pitch))
        {
            g.setColour (theme::accent().withAlpha (0.55f));
            g.fillRect (getWidth() - 5, y, 5, model.rowHeight - 1);
        }

        if (pitch % 12 == 0 || model.fold)
        {
            g.setColour (black ? theme::textDim() : juce::Colour (0xff4a5058));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (pitchName (pitch), 4, y, getWidth() - 10, model.rowHeight, juce::Justification::centredRight);
        }
    }
}

void KeyColumn::setPitch (int pitch)
{
    if (pitch == current) return;
    if (current >= 0 && model.onPreview) model.onPreview (current, false);
    current = pitch;
    if (current >= 0 && model.onPreview) model.onPreview (current, true);
    model.pressedPitch = current;
    repaint();
}

void KeyColumn::mouseDown (const juce::MouseEvent& e) { setPitch (model.rowToPitch (e.y / model.rowHeight)); }
void KeyColumn::mouseDrag (const juce::MouseEvent& e) { setPitch (model.rowToPitch (e.y / model.rowHeight)); }
void KeyColumn::mouseUp (const juce::MouseEvent&)     { setPitch (-1); }

// ============================================================ RulerBar

void RulerBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panelLight());

    const auto clipBox = g.getClipBounds();
    const double ppb = model.pixelsPerBeat;
    const int totalBeats = static_cast<int> (std::ceil (model.clip().length()));

    g.setColour (theme::accent().withAlpha (0.22f));
    g.fillRect (0.0f, 0.0f, static_cast<float> (model.gridWidthPixels()), 4.0f);   // faixa do clip/loop

    g.setFont (juce::FontOptions (11.0f));
    for (int b = 0; b <= totalBeats; ++b)
    {
        const float x = static_cast<float> (b * ppb);
        if (x < clipBox.getX() - 40 || x > clipBox.getRight() + 40) continue;
        const bool bar = (b % 4 == 0);
        g.setColour (bar ? theme::text() : theme::textDim());
        g.drawVerticalLine (static_cast<int> (x), bar ? 6.0f : 14.0f, static_cast<float> (getHeight()));
        if (bar)
            g.drawText (juce::String (b / 4 + 1), static_cast<int> (x) + 3, 4, 40, getHeight() - 6, juce::Justification::centredLeft);
        else if (ppb >= 40)
            g.drawText (juce::String (b / 4 + 1) + "." + juce::String (b % 4 + 1), static_cast<int> (x) + 3, 12, 40, getHeight() - 12, juce::Justification::centredLeft);
    }
}

void RulerBar::seek (const juce::MouseEvent& e)
{
    if (onSeek) onSeek (std::clamp (e.position.x / model.pixelsPerBeat, 0.0, model.clip().length()));
}

void RulerBar::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() < 4) return;
    if (! dragging) last = e.getEventRelativeTo (getParentComponent()).position;
    dragging = true;
    const auto now = e.getEventRelativeTo (getParentComponent()).position;     // posição estável durante o rolar
    const float dx = now.x - last.x, dy = now.y - last.y;
    last = now;
    if (onDrag) onDrag (dx, dy);
}

void RulerBar::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) seek (e);
    dragging = false;
}

// ============================================================ VelocityLane

int VelocityLane::velocityAt (float y) const
{
    const float h = static_cast<float> (getHeight() - 6);
    return std::clamp (static_cast<int> (std::lround ((1.0f - (y - 3.0f) / h) * 127.0f)), 1, 127);
}

void VelocityLane::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    const auto clipBox = g.getClipBounds();

    g.setColour (theme::border());
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));

    for (const auto& n : model.clip().notes())
    {
        const int x = static_cast<int> (n.startBeats * model.pixelsPerBeat);
        if (x < clipBox.getX() - 4 || x > clipBox.getRight() + 4) continue;

        const float h = static_cast<float> (getHeight() - 6) * static_cast<float> (n.velocity) / 127.0f;
        juce::Colour c = model.isSelected (n.id) ? theme::noteSelected() : theme::noteFill();
        if (n.muted) c = juce::Colour (0xff5b6169);
        g.setColour (c);
        g.fillRect (static_cast<float> (x), static_cast<float> (getHeight()) - 3.0f - h, 3.0f, h);
        g.fillEllipse (static_cast<float> (x) - 2.0f, static_cast<float> (getHeight()) - 3.0f - h - 2.0f, 7.0f, 7.0f);
    }

    g.setColour (theme::textDim());
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("Velocity", 4 + clipBox.getX() * 0, 2, 60, 12, juce::Justification::left);
}

void VelocityLane::mouseDown (const juce::MouseEvent& e)
{
    primaryId = -1;
    editBegun = false;
    originalVel.clear();

    int bestDist = 6;
    for (const auto& n : model.clip().notes())
    {
        const int dist = std::abs (static_cast<int> (n.startBeats * model.pixelsPerBeat) + 1 - e.x);
        if (dist <= bestDist) { bestDist = dist; primaryId = n.id; }
    }
    if (primaryId < 0) return;

    if (model.isSelected (primaryId))
        for (int id : model.selection) { if (auto* n = model.clip().find (id)) originalVel[id] = n->velocity; }
    else
        originalVel[primaryId] = model.clip().find (primaryId)->velocity;

    mouseDrag (e);
}

void VelocityLane::mouseDrag (const juce::MouseEvent& e)
{
    if (primaryId < 0 || originalVel.empty()) return;
    if (! editBegun) { model.beginEdit(); editBegun = true; }

    const int delta = velocityAt (e.position.y) - originalVel.at (primaryId);
    for (const auto& [id, v] : originalVel)
        if (auto* n = model.clip().find (id)) n->velocity = std::clamp (v + delta, 1, 127);

    model.notify (false);
}

void VelocityLane::mouseUp (const juce::MouseEvent&) { primaryId = -1; editBegun = false; originalVel.clear(); }

// ============================================================ PianoRollEditor

PianoRollEditor::PianoRollEditor (RollModel& m) : model (m)
{
    addAndMakeVisible (viewport);
    addAndMakeVisible (keyHolder);
    addAndMakeVisible (rulerHolder);
    addAndMakeVisible (velocityHolder);
    keyHolder.addAndMakeVisible (keys);
    rulerHolder.addAndMakeVisible (ruler);
    velocityHolder.addAndMakeVisible (velocity);

    viewport.setViewedComponent (&grid, false);
    viewport.setScrollBarsShown (true, true);
    viewport.onScroll = [this]
    {
        syncPositions();
        const int centerRow = (viewport.getViewPositionY() + viewport.getMaximumVisibleHeight() / 2) / model.rowHeight;
        const int p = model.rowToPitch (centerRow);
        if (p >= 0) lastCenterPitch = p;
    };
    ruler.onSeek = [this] (double b) { if (onSeek) onSeek (b); };
    ruler.onDrag = [this] (float dx, float dy)
    {
        model.pixelsPerBeat = std::clamp (model.pixelsPerBeat * std::exp (-static_cast<double> (dy) * 0.02), 12.0, 320.0);
        viewport.setViewPosition (std::max (0, viewport.getViewPositionX() - static_cast<int> (dx)), viewport.getViewPositionY());
        model.sendChangeMessage();
    };
    grid.onFocusChange = [this] { repaint(); };

    grid.updateSize();
    model.addChangeListener (this);
}

PianoRollEditor::~PianoRollEditor()
{
    model.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void PianoRollEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    if (grid.hasKeyboardFocus (false))               // anel de foco (navegação por teclado)
    {
        g.setColour (theme::accent());
        g.drawRect (getLocalBounds(), 2);
    }
}

void PianoRollEditor::zoomHorizontal (double factor)
{
    model.pixelsPerBeat = std::clamp (model.pixelsPerBeat * factor, 12.0, 320.0);
    model.sendChangeMessage();
}

void PianoRollEditor::zoomVertical (int deltaPixels)
{
    model.rowHeight = std::clamp (model.rowHeight + deltaPixels, 8, 32);
    model.sendChangeMessage();
}

void PianoRollEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (rulerHeight);
    top.removeFromLeft (keyWidth);
    rulerHolder.setBounds (top);

    auto bottom = area.removeFromBottom (velocityHeight);
    bottom.removeFromLeft (keyWidth);
    velocityHolder.setBounds (bottom);

    auto left = area.removeFromLeft (keyWidth);
    viewport.setBounds (area);
    keyHolder.setBounds (left.withHeight (viewport.getMaximumVisibleHeight()));

    grid.updateSize();
    syncPositions();

    if (! scrolledOnce && getHeight() > 0)
    {
        scrolledOnce = true;
        scrollToPitch (60);
    }
}

void PianoRollEditor::syncPositions()
{
    const auto vp = viewport.getViewPosition();
    keyHolder.setSize (keyWidth, viewport.getMaximumVisibleHeight());
    keys.setBounds (0, -vp.y, keyWidth, std::max (1, model.totalHeight()));
    ruler.setBounds (-vp.x, 0, std::max (1, static_cast<int> (model.gridWidthPixels()) + 1), rulerHeight);
    velocity.setBounds (-vp.x, 0, std::max (1, static_cast<int> (model.gridWidthPixels()) + 1), velocityHeight);
}

void PianoRollEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const int keepPitch = lastCenterPitch;
    const int oldHeight = grid.getHeight();
    grid.updateSize();
    if (grid.getHeight() != oldHeight)                     // fold/escala mudou as linhas
    {
        int best = -1, bestDist = 1000;
        for (int r = 0; r < static_cast<int> (model.rows().size()); ++r)
        {
            const int d = std::abs (model.rowToPitch (r) - keepPitch);
            if (d < bestDist) { bestDist = d; best = r; }
        }
        if (best >= 0)
            viewport.setViewPosition (viewport.getViewPositionX(),
                                      std::max (0, best * model.rowHeight - viewport.getMaximumVisibleHeight() / 2));
        lastCenterPitch = keepPitch;
    }
    syncPositions();
    grid.repaint();
    keys.repaint();
    ruler.repaint();
    velocity.repaint();
}

void PianoRollEditor::setPlayhead (double beats)
{
    grid.setPlayhead (beats);
}

void PianoRollEditor::scrollToPitch (int pitch)
{
    const int row = model.pitchToRow (pitch);
    if (row < 0) return;
    const int y = std::max (0, row * model.rowHeight - viewport.getMaximumVisibleHeight() / 2);
    viewport.setViewPosition (viewport.getViewPositionX(), y);
}
