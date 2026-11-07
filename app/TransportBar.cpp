#include "TransportBar.h"
#include <cmath>

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool active = (icon == Icon::Pause);        // "pause" aparece enquanto toca
    g.setColour (active ? theme::accent().withAlpha (0.85f)
                        : (down ? theme::panelLight().brighter (0.2f) : over ? theme::panelLight().brighter (0.1f) : theme::panelLight()));
    g.fillRoundedRectangle (r, 4.0f);

    auto c = r.reduced (r.getWidth() * 0.3f, r.getHeight() * 0.28f);
    g.setColour (active ? juce::Colours::black : theme::text());
    switch (icon)
    {
        case Icon::Play:
        {
            juce::Path p;
            p.addTriangle (c.getX(), c.getY(), c.getX(), c.getBottom(), c.getRight(), c.getCentreY());
            g.fillPath (p);
            break;
        }
        case Icon::Pause:
        {
            const float w = c.getWidth() * 0.34f;
            g.fillRect (c.getX(), c.getY(), w, c.getHeight());
            g.fillRect (c.getRight() - w, c.getY(), w, c.getHeight());
            break;
        }
        case Icon::Stop:
            g.fillRect (c);
            break;
        case Icon::Rewind:
        {
            const float w = c.getWidth() * 0.22f;
            g.fillRect (c.getX(), c.getY(), w, c.getHeight());
            juce::Path p;
            p.addTriangle (c.getRight(), c.getY(), c.getRight(), c.getBottom(), c.getX() + w + 1.0f, c.getCentreY());
            g.fillPath (p);
            break;
        }
    }
}

TransportBar::TransportBar (SequencerGraph& g) : graph (g)
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (rewindButton);
    for (auto* b : std::initializer_list<juce::Component*> { &playButton, &stopButton, &rewindButton }) b->setMouseClickGrabsKeyboardFocus (false);
    loopButton.setMouseClickGrabsKeyboardFocus (false);
    rewindButton.onClick = [this] { rewind(); };
    addAndMakeVisible (loopButton);
    addAndMakeVisible (bpmLabel);
    addAndMakeVisible (bpmCaption);
    addAndMakeVisible (positionLabel);

    playButton.onClick = [this] { togglePlay(); };
    stopButton.onClick = [this] { stop(); };

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (graph.isLoop(), juce::dontSendNotification);
    loopButton.setColour (juce::TextButton::buttonOnColourId, theme::accent());
    loopButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    loopButton.onClick = [this] { graph.setLoop (loopButton.getToggleState()); if (onLoopToggled) onLoopToggled (loopButton.getToggleState()); };

    bpmLabel.setEditable (false, true, false);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::backgroundColourId, theme::panelLight());
    bpmLabel.setColour (juce::Label::textColourId, theme::text());
    bpmLabel.setText (juce::String (graph.getBpm(), 2), juce::dontSendNotification);
    bpmLabel.onTextChange = [this]
    {
        const double v = std::clamp (bpmLabel.getText().getDoubleValue(), 20.0, 999.0);
        graph.setBpm (v);
        bpmLabel.setText (juce::String (v, 2), juce::dontSendNotification);
    };

    bpmCaption.setColour (juce::Label::textColourId, theme::textDim());
    bpmCaption.setFont (juce::FontOptions (11.0f));

    positionLabel.setJustificationType (juce::Justification::centred);
    positionLabel.setColour (juce::Label::backgroundColourId, theme::panelLight());
    positionLabel.setColour (juce::Label::textColourId, theme::text());
    positionLabel.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain));

    startTimerHz (30);
    timerCallback();
}

void TransportBar::togglePlay()
{
    graph.setPlaying (! graph.isPlaying());
    updateButtons();
}

void TransportBar::rewind() { graph.rewind(); }

void TransportBar::stop()
{
    graph.setPlaying (false);
    graph.rewind();
    updateButtons();
}

void TransportBar::updateButtons()
{
    playButton.setIcon (graph.isPlaying() ? IconButton::Icon::Pause : IconButton::Icon::Play);
}

void TransportBar::timerCallback()
{
    updateButtons();
    const double pos = graph.positionBeats();
    if (std::abs (pos - lastPosition) < 1.0e-9) return;
    lastPosition = pos;

    const int bar   = static_cast<int> (std::floor (pos / 4.0)) + 1;
    const int beat  = static_cast<int> (std::floor (std::fmod (pos, 4.0))) + 1;
    const int sixt  = static_cast<int> (std::floor (std::fmod (pos, 1.0) * 4.0)) + 1;
    positionLabel.setText (juce::String (bar) + " . " + juce::String (beat) + " . " + juce::String (sixt), juce::dontSendNotification);

    if (onPosition) onPosition (pos);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    g.setColour (theme::border());
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    bpmCaption.setBounds (area.removeFromLeft (34));
    bpmLabel.setBounds (area.removeFromLeft (76));
    area.removeFromLeft (16);
    rewindButton.setBounds (area.removeFromLeft (40));
    area.removeFromLeft (6);
    playButton.setBounds (area.removeFromLeft (44));
    area.removeFromLeft (6);
    stopButton.setBounds (area.removeFromLeft (44));
    area.removeFromLeft (12);
    loopButton.setBounds (area.removeFromLeft (56));
    area.removeFromLeft (16);
    positionLabel.setBounds (area.removeFromLeft (120));
}
