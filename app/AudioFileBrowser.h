#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

class AudioFileBrowser : public juce::Component
{
public:
    AudioFileBrowser();
    std::function<void (const juce::File&)> onPreviewRequested;
    std::function<void()> onPreviewStopRequested;
    void setStatus (const juce::String& text) { status.setText (text, juce::dontSendNotification); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class FileTreeItem;
    void populate (FileTreeItem&);
    void selectFile (const juce::File&);
    static bool isAudioFile (const juce::File&);

    juce::Label title { {}, "Arquivos de audio" };
    juce::Label status { {}, "Selecione um arquivo de audio para ouvir" };
    juce::TreeView tree;
    juce::TextButton previewButton { "Ouvir" }, stopButton { "Parar" };
    juce::File selectedFile;
};
