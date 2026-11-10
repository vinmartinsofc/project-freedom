#include "AudioClipEditor.h"
#include <algorithm>
#include <cmath>

AudioClipEditor::AudioClipEditor()
{
    addAndMakeVisible (fileName);
    addAndMakeVisible (tempoInfo);
    addAndMakeVisible (bpmLabel);
    addAndMakeVisible (sourceBpm);
    addAndMakeVisible (closeButton);

    fileName.setColour (juce::Label::textColourId, theme::text());
    fileName.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    tempoInfo.setColour (juce::Label::textColourId, theme::textDim());
    bpmLabel.setColour (juce::Label::textColourId, theme::textDim());
    sourceBpm.setRange (20.0, 300.0, 0.1);
    sourceBpm.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
    sourceBpm.setColour (juce::Slider::textBoxTextColourId, theme::text());
    sourceBpm.setColour (juce::Slider::textBoxBackgroundColourId, theme::panelLight());
    closeButton.setMouseClickGrabsKeyboardFocus (false);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    sourceBpm.onValueChange = [this]
    {
        if (clip == nullptr || ! onSourceBpmChanged)
            return;
        const double value = sourceBpm.getValue();
        if (! onSourceBpmChanged (value))
        {
            sourceBpm.setValue (clip->sourceBpm, juce::dontSendNotification);
            tempoInfo.setText ("BPM nao aplicado: os clips se sobrepoem.", juce::dontSendNotification);
        }
    };
}

void AudioClipEditor::setClip (const roll::AudioPlacement* selected,
                               std::shared_ptr<const AudioFileData> audio,
                               double bpm, double beatGrid)
{
    clip = selected;
    source = std::move (audio);
    projectBpm = bpm;
    gridBeats = beatGrid;
    fileName.setText (clip != nullptr ? juce::String (clip->name) : "Nenhum audio selecionado",
                      juce::dontSendNotification);
    sourceBpm.setValue (clip != nullptr ? clip->sourceBpm : 120.0, juce::dontSendNotification);
    sourceBpm.setEnabled (clip != nullptr && clip->durationSeconds > 0.0);
    tempoInfo.setText (clip != nullptr
        ? "Projeto: " + juce::String (projectBpm, 1) + " BPM    |    Duracao: "
            + juce::String (clip->durationSeconds, 2) + " s"
        : juce::String(), juce::dontSendNotification);
    repaint();
}

void AudioClipEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel());
    auto waveformArea = getLocalBounds().reduced (8).withTrimmedTop (42);
    if (waveformArea.isEmpty())
        return;

    g.setColour (theme::panelLight());
    g.fillRoundedRectangle (waveformArea.toFloat(), 4.0f);
    g.saveState();
    g.reduceClipRegion (waveformArea);

    const double beats = clip != nullptr ? std::max (clip->lengthBeats, 0.0) : 0.0;
    if (beats > 0.0 && gridBeats > 0.0)
    {
        const int subdivisions = std::min (4096, static_cast<int> (std::ceil (beats / gridBeats)));
        for (int i = 0; i <= subdivisions; ++i)
        {
            const double beat = i * gridBeats;
            if (beat > beats)
                break;
            const bool bar = std::abs (std::remainder (beat, 4.0)) < 1.0e-6;
            const bool wholeBeat = std::abs (std::remainder (beat, 1.0)) < 1.0e-6;
            g.setColour (theme::border().withAlpha (bar ? 0.9f : wholeBeat ? 0.65f : 0.35f));
            const int x = waveformArea.getX() + static_cast<int> (beat / beats * waveformArea.getWidth());
            g.drawVerticalLine (x, static_cast<float> (waveformArea.getY()),
                                static_cast<float> (waveformArea.getBottom()));
        }
    }

    const float center = static_cast<float> (waveformArea.getCentreY());
    const float halfHeight = static_cast<float> (waveformArea.getHeight()) * 0.42f;
    g.setColour (theme::accent().brighter (0.25f));
    g.drawHorizontalLine (static_cast<int> (center), static_cast<float> (waveformArea.getX()),
                          static_cast<float> (waveformArea.getRight()));
    if (source != nullptr && ! source->waveform.empty())
    {
        const auto count = source->waveform.size();
        for (size_t i = 0; i < count; ++i)
        {
            const float x = static_cast<float> (waveformArea.getX())
                          + static_cast<float> (i) / static_cast<float> (count) * waveformArea.getWidth();
            const float amplitude = std::clamp (source->waveform[i], 0.0f, 1.0f) * halfHeight;
            g.drawVerticalLine (static_cast<int> (x), center - amplitude, center + amplitude);
        }
    }
    g.restoreState();
}

void AudioClipEditor::resized()
{
    auto header = getLocalBounds().reduced (8).removeFromTop (34);
    closeButton.setBounds (header.removeFromRight (110));
    header.removeFromRight (8);
    sourceBpm.setBounds (header.removeFromRight (126));
    header.removeFromRight (6);
    bpmLabel.setBounds (header.removeFromRight (90));
    tempoInfo.setBounds (header.removeFromRight (205));
    fileName.setBounds (header);
}
