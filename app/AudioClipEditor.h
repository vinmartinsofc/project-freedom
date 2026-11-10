#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "Theme.h"
#include <functional>

class AudioClipEditor : public juce::Component
{
public:
    AudioClipEditor();
    void setClip (const roll::AudioPlacement*, std::shared_ptr<const AudioFileData>,
                  double projectBpm, double gridBeats);

    std::function<bool (double sourceBpm)> onSourceBpmChanged;
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const roll::AudioPlacement* clip = nullptr;
    std::shared_ptr<const AudioFileData> source;
    double projectBpm = 120.0, gridBeats = 0.25;
    juce::Label fileName, tempoInfo, bpmLabel { {}, "BPM da origem" };
    juce::Slider sourceBpm;
    juce::TextButton closeButton { "Fechar editor" };
};
