#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "AudioEngine.h"
#include "AudioFileBrowser.h"
#include "ArrangementView.h"
#include "BrowserPanel.h"
#include "DeviceView.h"
#include "PianoRollEditor.h"
#include "PluginScanner.h"
#include "ProjectModel.h"
#include "RollModel.h"
#include "TransportBar.h"
#include <map>

class MainComponent : public juce::Component, public juce::DragAndDropContainer,
                      private juce::ChangeListener, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    class ActionIndicator;
    class PluginWindow;

    void loadDevice (const juce::PluginDescription&);
    void openDeviceEditor (int trackId, int deviceId);
    void updateActionIndicator();
    void setBrowserVisible (bool);
    void setAudioBrowserVisible (bool);
    void setFavoritesVisible (bool);
    void setPianoRollVisible (bool);
    void loadAudioFile (const juce::File&, int trackId, double startBeat);
    void publishAudioTrack (int trackId);
    bool selectedTrackSpan (double& start, double& end) const;
    void playSelectedTrack();
    void loopSelectedTrack();
    void closeWindowsOfTrack (int trackId);
    void refreshDeviceView();
    void applyMixers();
    void applyScale();
    void applyLength();
    void onSelectionChanged();
    void showDetail (bool devices);
    void cycleFocus();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    juce::String deviceSummary (int trackId) const;

    AudioEngine engine;
    PluginScanner scanner { engine.formats() };
    ProjectModel project;
    RollModel model;

    TransportBar transport { engine.graph() };
    BrowserPanel browser { scanner };
    AudioFileBrowser audioBrowser;
    ArrangementView arrangement { project };
    PianoRollEditor editor { model };
    DeviceView deviceView;

    // painel do clip
    juce::TextButton clipTab { "Clip" }, deviceTab { "Dispositivos" };
    juce::Label scaleCaption { {}, "Escala" }, lengthCaption { {}, "Tamanho" }, gridCaption { {}, "Grade" }, zoomCaption { {}, "Zoom" };
    juce::ComboBox rootBox, modeBox, lengthBox;
    juce::TextButton scaleButton { "Scale" }, highlightButton { "Destacar (K)" }, foldButton { "Fold" }, foldScaleButton { "Fold escala" },
                     drawButton { "Draw (B)" }, snapButton { "Snap" }, previewButton { "Preview" },
                     zoomHMinus { "H -" }, zoomHPlus { "H +" }, zoomVMinus { "V -" }, zoomVPlus { "V +" };
    juce::Label gridLabel;
    std::vector<juce::Component*> clipControls;

    std::map<int, std::unique_ptr<PluginWindow>> windows;
    std::map<int, juce::String> pendingPluginLoads;
    std::map<std::string, std::shared_ptr<const AudioFileData>> audioAssets;
    std::shared_ptr<std::atomic<bool>> lifetimeToken { std::make_shared<std::atomic<bool>> (true) };
    std::unique_ptr<ActionIndicator> actionIndicator;
    int nextPluginCascadeIndex = 0;
    int nextPluginLoadToken = 0;
    int previewLoadToken = 0;
    int pendingAudioLoads = 0;
    bool scanningPlugins = false;
    juce::String scanMessage;

    bool showingDevices = false;
    bool pianoRollVisible = true;
    double arrangementRatio = 0.42;
    juce::Rectangle<int> splitterRect;
    int rightTop = 0, rightHeight = 1;
    bool draggingSplitter = false, draggingBrowserSplitter = false;
    std::uint64_t lastLo = 0, lastHi = 0;
    int browserWidth = 300;
    juce::Rectangle<int> browserSplitterRect;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
