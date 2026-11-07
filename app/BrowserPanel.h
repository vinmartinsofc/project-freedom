#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginScanner.h"
#include "Theme.h"
#include <functional>
#include <vector>

// Lista de instrumentos detectados. Duplo clique (ou Enter) carrega na trilha.
class BrowserPanel : public juce::Component,
                     private juce::ListBoxModel,
                     private juce::ChangeListener
{
public:
    explicit BrowserPanel (PluginScanner&);
    ~BrowserPanel() override;

    std::function<void (const juce::PluginDescription&)> onLoadRequested;

    void setStatus (const juce::String& text) { status.setText (text, juce::dontSendNotification); }
    void refresh();
    void grabFocus() { listBox.grabKeyboardFocus(); }
    int  shownCount() const { return static_cast<int> (shown.size()); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int  getNumRows() override { return static_cast<int> (shown.size()); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { load (row); }
    void returnKeyPressed (int row) override { load (row); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void load (int row);

    PluginScanner& scanner;
    juce::Label title { {}, "Plugins" };
    juce::TextEditor search;
    juce::ListBox listBox { "plugins", this };
    juce::TextButton rescanButton { "Rescan" };
    juce::TextButton allButton { "Todos" }, instrumentsButton { "Instrumentos" }, effectsButton { "Efeitos" };
    enum class Filter { All, Instruments, Effects } filter = Filter::All;
    void setFilter (Filter f);
    juce::Label status;
    std::vector<juce::PluginDescription> shown;
};
