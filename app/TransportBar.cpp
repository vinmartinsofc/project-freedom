#include "TransportBar.h"
#include <cmath>

void IconButton::setIcon (icons::Icon i)
{
    if (icon == i)
        return;

    icon = i;
    const auto name = icon == icons::Icon::Pause ? "Pausar" : "Reproduzir";
    setName (name);
    setTooltip (name);
    repaint();
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool active = (icon == icons::Icon::Pause);
    g.setColour (down ? theme::pressed() : over ? theme::hover() : theme::control());
    g.fillRoundedRectangle (r, 4.0f);

    if (active)
    {
        g.setColour (theme::accent());
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    const auto iconColour = ! isEnabled() ? theme::textDim().withAlpha (0.4f)
                                         : active ? theme::accent()
                                                  : over ? juce::Colours::white : theme::text();
    if (const auto* drawable = icons::get (icon, iconColour, 24))
        drawable->drawWithin (g, r.reduced (8.0f, 6.0f), juce::RectanglePlacement::centred, 1.0f);
    else
    {
        g.setColour (iconColour);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText ("?", r, juce::Justification::centred);
    }
}

TransportBar::TransportBar (SequencerGraph& g) : graph (g)
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (rewindButton);
    for (auto* b : std::initializer_list<juce::Component*> { &playButton, &stopButton, &rewindButton }) b->setMouseClickGrabsKeyboardFocus (false);
    loopButton.setMouseClickGrabsKeyboardFocus (false);
    browserButton.setMouseClickGrabsKeyboardFocus (false);
    fileBrowserButton.setMouseClickGrabsKeyboardFocus (false);
    pianoRollButton.setMouseClickGrabsKeyboardFocus (false);
    rewindButton.onClick = [this] { rewind(); };
    addAndMakeVisible (loopButton);
    addAndMakeVisible (browserButton);
    addAndMakeVisible (fileBrowserButton);
    addAndMakeVisible (pianoRollButton);
    addAndMakeVisible (bpmLabel);
    addAndMakeVisible (bpmCaption);
    addAndMakeVisible (positionLabel);

    playButton.onClick = [this] { togglePlay(); };
    stopButton.onClick = [this] { stop(); };
    loopButton.setTooltip ("Ativar ou desativar o loop");

    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (graph.isLoop(), juce::dontSendNotification);
    loopButton.setColour (juce::TextButton::buttonOnColourId, theme::accent());
    loopButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    loopButton.onClick = [this] { graph.setLoop (loopButton.getToggleState()); if (onLoopToggled) onLoopToggled (loopButton.getToggleState()); };

    browserButton.setTooltip ("Mostrar ou ocultar o navegador de plugins (Ctrl+B)");
    browserButton.setClickingTogglesState (true);
    browserButton.setToggleState (true, juce::dontSendNotification);
    browserButton.setColour (juce::TextButton::buttonOnColourId, theme::accent());
    browserButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    browserButton.onClick = [this]
    {
        if (onBrowserToggled)
            onBrowserToggled (browserButton.getToggleState());
    };

    fileBrowserButton.setTooltip ("Mostrar ou ocultar o navegador de arquivos de audio");
    fileBrowserButton.setClickingTogglesState (true);
    fileBrowserButton.setToggleState (false, juce::dontSendNotification);
    fileBrowserButton.setColour (juce::TextButton::buttonOnColourId, theme::accent());
    fileBrowserButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    fileBrowserButton.onClick = [this]
    {
        if (onFileBrowserToggled)
            onFileBrowserToggled (fileBrowserButton.getToggleState());
    };

    pianoRollButton.setTooltip ("Mostrar ou ocultar o painel inferior do piano roll e dispositivos");
    pianoRollButton.setClickingTogglesState (true);
    pianoRollButton.setToggleState (true, juce::dontSendNotification);
    pianoRollButton.setColour (juce::TextButton::buttonOnColourId, theme::accent());
    pianoRollButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    pianoRollButton.onClick = [this]
    {
        if (onPianoRollToggled)
            onPianoRollToggled (pianoRollButton.getToggleState());
    };

    bpmLabel.setEditable (false, true, false);
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::backgroundColourId, theme::control());
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
    positionLabel.setColour (juce::Label::backgroundColourId, theme::control());
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

void TransportBar::setBrowserVisible (bool visible)
{
    browserButton.setToggleState (visible, juce::dontSendNotification);
}

void TransportBar::setFileBrowserVisible (bool visible)
{
    fileBrowserButton.setToggleState (visible, juce::dontSendNotification);
}

void TransportBar::setPianoRollVisible (bool visible)
{
    pianoRollButton.setToggleState (visible, juce::dontSendNotification);
}

void TransportBar::updateButtons()
{
    playButton.setIcon (graph.isPlaying() ? icons::Icon::Pause : icons::Icon::Play);
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
    auto area = getLocalBounds().reduced (8, 4);
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
    area.removeFromLeft (12);
    browserButton.setBounds (area.removeFromRight (88));
    area.removeFromRight (6);
    fileBrowserButton.setBounds (area.removeFromRight (72));
    area.removeFromRight (6);
    pianoRollButton.setBounds (area.removeFromRight (104));
}
