#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginScanner.h"
#include "Theme.h"
#include <functional>
#include <vector>

// Lista de instrumentos detectados. Duplo clique (ou Enter) carrega na trilha.
class BrowserPanel : public juce::Component,
                     private juce::ChangeListener,
                     private juce::KeyListener
{
public:
    explicit BrowserPanel (PluginScanner&);
    ~BrowserPanel() override;

    std::function<void (const juce::PluginDescription&)> onLoadRequested;

    void setStatus (const juce::String& text) { status.setText (text, juce::dontSendNotification); }
    void refresh();
    void grabFocus();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class TreeItem;
    class PluginTree;

    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool loadSelectedPlugin();
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void load (const juce::PluginDescription&);
    void saveTreeState();
    juce::File treeStateFile() const;

    PluginScanner& scanner;
    juce::Label title { {}, "Plugins" };
    juce::TextEditor search;
    std::unique_ptr<PluginTree> tree;
    juce::TextButton rescanButton { "Rescan" };
    juce::TextButton allButton { "Todos" }, instrumentsButton { "Instrumentos" }, effectsButton { "Efeitos" };
    enum class Filter { All, Instruments, Effects } filter = Filter::All;
    void setFilter (Filter f);
    juce::Label status;
    std::unique_ptr<juce::XmlElement> opennessBeforeSearch;
    bool searchActive = false, rebuildingTree = false;
};
