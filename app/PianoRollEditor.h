#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "RollModel.h"
#include "Theme.h"
#include <functional>
#include <map>

// Grade de notas: desenha o clip e trata mouse/teclado de edição.
class NoteGrid : public juce::Component
{
public:
    explicit NoteGrid (RollModel& m) : model (m) { setWantsKeyboardFocus (true); }

    std::function<void()> onFocusChange;
    std::function<void (double)> onZoomHorizontal;
    void focusGained (FocusChangeType) override { if (onFocusChange) onFocusChange(); }
    void focusLost (FocusChangeType) override   { if (onFocusChange) onFocusChange(); }

    void updateSize();
    void setPlayhead (double beats);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { None, Move, Resize, Rubber };

    float beatsToX (double b) const  { return static_cast<float> (b * model.pixelsPerBeat); }
    double xToBeats (float x) const  { return x / model.pixelsPerBeat; }
    int   yToRow (float y) const     { return static_cast<int> (std::floor (y / model.rowHeight)); }
    juce::Rectangle<float> noteRect (const roll::Note&) const;
    int  hitNote (juce::Point<float>) const;
    void updateCursor (juce::Point<float>);
    void setPreview (int pitch);
    void flashPreview (int pitch);

    RollModel& model;
    double playheadBeats = 0.0;

    Drag   drag = Drag::None;
    bool   editBegun = false;
    bool   copied = false;                    // Ctrl+arrastar: cópias já deixadas para trás
    int    primaryId = -1;
    double downBeat = 0.0;
    std::map<int, roll::Note> original;       // notas selecionadas no início do gesto
    juce::Point<float> rubberStart;
    juce::Rectangle<float> rubberRect;
    std::set<int> baseSelection;
    int previewPitch = -1;
};

// Coluna de teclas à esquerda (clique para pré-escutar).
class KeyColumn : public juce::Component
{
public:
    explicit KeyColumn (RollModel& m) : model (m) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void setPitch (int pitch);
    RollModel& model;
    int current = -1;
};

// Régua com compassos/beats e a faixa do clip. Clique reposiciona o playhead.
class RulerBar : public juce::Component
{
public:
    explicit RulerBar (RollModel& m) : model (m) {}
    std::function<void (double)> onSeek;
    std::function<void (float dx, float dy)> onDrag;    // arrastar: vertical = zoom, horizontal = rolar
    std::function<void (double)> onZoom;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void seek (const juce::MouseEvent&);
    bool isResizeHandle (juce::Point<float>) const;
    RollModel& model;
    juce::Point<float> last;
    bool dragging = false, resizing = false;
};

// Faixa de velocity: uma barra por nota; arraste para mudar.
class VelocityLane : public juce::Component
{
public:
    explicit VelocityLane (RollModel& m) : model (m) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    int velocityAt (float y) const;
    RollModel& model;
    int primaryId = -1;
    bool editBegun = false;
    std::map<int, int> originalVel;
};

class SyncViewport : public juce::Viewport
{
public:
    std::function<void()> onScroll;
    void visibleAreaChanged (const juce::Rectangle<int>& a) override
    {
        juce::Viewport::visibleAreaChanged (a);
        if (onScroll) onScroll();
    }
};

// Piano roll completo: régua + teclas + grade rolável + velocity.
class PianoRollEditor : public juce::Component, private juce::ChangeListener
{
public:
    explicit PianoRollEditor (RollModel&);
    ~PianoRollEditor() override;

    void setPlayhead (double beats);
    void scrollToPitch (int pitch);
    void zoomHorizontal (double factor);
    void zoomVertical (int deltaPixels);
    void grabFocus() { grid.grabKeyboardFocus(); }
    void repaintKeys() { keys.repaint(); }
    std::function<void (double)> onSeek;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Holder : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override { g.fillAll (theme::panel()); }
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void syncPositions();
    double constrainedHorizontalZoom (double requested) const;

    static constexpr int keyWidth = 58, rulerHeight = 24, velocityHeight = 72;

    RollModel& model;
    NoteGrid grid { model };
    KeyColumn keys { model };
    RulerBar ruler { model };
    VelocityLane velocity { model };
    SyncViewport viewport;
    Holder keyHolder, rulerHolder, velocityHolder;
    bool scrolledOnce = false;
    int  lastCenterPitch = 60;      // mantém a posição vertical ao ligar/desligar o fold
};
