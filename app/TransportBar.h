#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "Icons.h"
#include "Theme.h"
#include <functional>

// Botão vetorial com estados visuais e nome acessível.
class IconButton : public juce::Button
{
public:
    IconButton (icons::Icon i, const juce::String& name) : juce::Button (name), icon (i)
    {
        setTooltip (name);
    }
    void setIcon (icons::Icon i);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    icons::Icon icon;
};

// Barra de transporte: play/pause, stop, loop, BPM e posição (compasso.beat.16avo).
class TransportBar : public juce::Component, private juce::Timer
{
public:
    explicit TransportBar (SequencerGraph&);
    ~TransportBar() override { stopTimer(); }

    void togglePlay();
    void stop();
    void rewind();                      // volta ao início sem mudar play/pause
    std::function<void (bool)> onLoopToggled;
    std::function<void (double beats)> onPosition;   // chamado ~30x/s com o playhead

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateButtons();

    SequencerGraph& graph;
    IconButton playButton { icons::Icon::Play, "Reproduzir" };
    IconButton stopButton { icons::Icon::Stop, "Parar" };
    IconButton rewindButton { icons::Icon::Rewind, "Voltar ao início" };
    juce::TextButton loopButton { "Loop" };
    juce::Label bpmLabel, positionLabel, bpmCaption { {}, "BPM" };
    double lastPosition = -1.0;
};
