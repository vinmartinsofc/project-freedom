#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AudioEngine.h"
#include "Theme.h"
#include <functional>
#include <memory>
#include <vector>

// Cadeia de dispositivos (plugins) da trilha selecionada: instrumento primeiro, efeitos depois.
class DeviceView : public juce::Component
{
public:
    DeviceView();
    ~DeviceView() override;

    std::function<void (int trackId, int deviceId)> onOpen, onRemove;
    std::function<void (int trackId, int deviceId, bool bypassed)> onBypass;

    void setDevices (int trackId, const juce::String& trackName, const std::vector<DeviceInfo>& devices);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Box;
    int trackId = -1;
    juce::String trackName;
    std::vector<std::unique_ptr<Box>> boxes;
};
