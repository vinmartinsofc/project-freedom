#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "Theme.h"
#include <functional>

// Botão com ícone desenhado (play, pause, stop): sem depender de fontes/emoji.
class IconButton : public juce::Button
{
public:
    enum class Icon { Play, Pause, Stop, Rewind };
    explicit IconButton (Icon i) : juce::Button ("icon"), icon (i) {}
    void setIcon (Icon i) { icon = i; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    Icon icon;
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
    IconButton playButton { IconButton::Icon::Play }, stopButton { IconButton::Icon::Stop }, rewindButton { IconButton::Icon::Rewind };
    juce::TextButton loopButton { "Loop" };
    juce::Label bpmLabel, positionLabel, bpmCaption { {}, "BPM" };
    double lastPosition = -1.0;
};
