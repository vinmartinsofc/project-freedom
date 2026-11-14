#include "MainComponent.h"
#include "WindowPlacement.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace
{
double sourceBpmFromFileName (const juce::File& file, double fallback)
{
    const auto name = file.getFileNameWithoutExtension().toLowerCase();
    const int marker = name.indexOf ("bpm");
    if (marker <= 0)
        return fallback;

    int numberEnd = marker;
    while (numberEnd > 0 && (name[numberEnd - 1] == ' ' || name[numberEnd - 1] == '_' || name[numberEnd - 1] == '-'))
        --numberEnd;
    int start = numberEnd;
    while (start > 0)
    {
        const auto character = name[start - 1];
        if (character < '0' || character > '9')
            break;
        --start;
    }

    const double bpm = name.substring (start, numberEnd).getDoubleValue();
    return bpm >= 20.0 && bpm <= 300.0 ? bpm : fallback;
}

}

class MainComponent::ActionIndicator final : public juce::Component
{
public:
    ActionIndicator() : spinner (progress, juce::ProgressBar::Style::circular)
    {
        spinner.setPercentageDisplay (false);
        spinner.setColour (juce::ProgressBar::backgroundColourId, theme::border());
        spinner.setColour (juce::ProgressBar::foregroundColourId, theme::accent());
        addAndMakeVisible (spinner);

        message.setColour (juce::Label::textColourId, theme::text());
        message.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        message.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (message);
        setInterceptsMouseClicks (false, false);
    }

    void showMessage (const juce::String& text)
    {
        message.setText (text, juce::dontSendNotification);
        const bool wasVisible = isVisible();
        setVisible (true);
        if (! wasVisible)
            toFront (false);
        repaint();
    }

    void hide() { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::panel().withAlpha (0.97f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 7.0f);
        g.setColour (theme::border());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 7.0f, 1.0f);
    }

    void resized() override
    {
        spinner.setBounds (10, 10, 24, 24);
        message.setBounds (42, 0, getWidth() - 50, getHeight());
    }

private:
    double progress = -1.0;
    juce::ProgressBar spinner;
    juce::Label message;
};

// Janela de plugin: mantém o editor visível e oferece rolagem como fallback.
class MainComponent::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (const juce::String& name,
                  std::unique_ptr<juce::AudioProcessorEditor> editor,
                  const juce::Rectangle<int>& displayUserArea,
                  int cascadeIndex)
        : DocumentWindow (name, juce::Colours::darkgrey, DocumentWindow::minimiseButton | DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        setResizable (false, false);
        const int horizontalMargin = std::min (16, std::max (0, (displayUserArea.getWidth() - 1) / 2));
        const int verticalMargin = std::min (60, std::max (0, (displayUserArea.getHeight() - 1) / 2));
        const auto available = displayUserArea.reduced (horizontalMargin, verticalMargin);
        const auto scale = std::min ({ 1.0f,
                                       static_cast<float> (available.getWidth()) / std::max (1, editor->getWidth()),
                                       static_cast<float> (available.getHeight()) / std::max (1, editor->getHeight()) });

        if (scale >= 0.6f && scale < 1.0f)
            editor->setScaleFactor (scale);

        const bool needsViewport = editor->getWidth() > available.getWidth()
                                || editor->getHeight() > available.getHeight();
        if (needsViewport)
        {
            auto viewport = std::make_unique<juce::Viewport>();
            viewport->setScrollBarsShown (true, true);
            viewport->setSize (std::min (editor->getWidth(), available.getWidth()),
                               std::min (editor->getHeight(), available.getHeight()));
            viewport->setViewedComponent (editor.release(), true);
            setContentOwned (viewport.release(), true);
        }
        else
        {
            setContentOwned (editor.release(), true);
        }

        const auto placeWindow = [this, &displayUserArea, cascadeIndex]
        {
            return roll::WindowPlacement::placePluginWindow (
                { 0, 0, getWidth(), getHeight() },
                { displayUserArea.getX(), displayUserArea.getY(), displayUserArea.getWidth(), displayUserArea.getHeight() },
                cascadeIndex);
        };
        auto bounds = placeWindow();
        setBounds (bounds.x, bounds.y, bounds.width, bounds.height);
        setVisible (true);
        bounds = placeWindow();
        setBounds (bounds.x, bounds.y, bounds.width, bounds.height);
    }
    void closeButtonPressed() override { setVisible (false); }
};

MainComponent::MainComponent()
{
    addAndMakeVisible (transport);
    addAndMakeVisible (browser);
    addAndMakeVisible (audioBrowser);
    audioBrowser.setVisible (false);
    addAndMakeVisible (arrangement);
    addAndMakeVisible (editor);
    addAndMakeVisible (audioClipEditor);
    audioClipEditor.setVisible (false);
    addAndMakeVisible (deviceView);
    addAndMakeVisible (clipTab);
    addAndMakeVisible (deviceTab);
    actionIndicator = std::make_unique<ActionIndicator>();
    addAndMakeVisible (*actionIndicator);

    clipControls = { &scaleCaption, &lengthCaption, &gridCaption, &zoomCaption, &rootBox, &modeBox, &lengthBox, &scaleButton,
                     &highlightButton, &foldButton, &foldScaleButton, &drawButton, &snapButton, &previewButton,
                     &zoomHMinus, &zoomHPlus, &zoomVMinus, &zoomVPlus, &gridLabel, &editor };
    for (auto* c : clipControls) addAndMakeVisible (c);

    for (auto* l : { &scaleCaption, &lengthCaption, &gridCaption, &zoomCaption })
    {
        l->setColour (juce::Label::textColourId, theme::textDim());
        l->setFont (juce::FontOptions (11.0f));
    }
    gridLabel.setColour (juce::Label::textColourId, theme::text());

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) rootBox.addItem (names[i], i + 1);
    rootBox.setSelectedId (1, juce::dontSendNotification);
    for (int m = 0; m <= static_cast<int> (roll::Mode::MelodicMinor); ++m)
        modeBox.addItem (juce::String (roll::Scale::nameOf (static_cast<roll::Mode> (m)).data()), m + 1);
    modeBox.setSelectedId (1, juce::dontSendNotification);
    for (int bars : { 1, 2, 4, 8, 16, 32 }) lengthBox.addItem (juce::String (bars) + (bars == 1 ? " compasso" : " compassos"), bars);

    for (auto* b : { &scaleButton, &highlightButton, &foldButton, &foldScaleButton, &drawButton, &snapButton, &previewButton })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent());
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
    for (auto* b : { &clipTab, &deviceTab })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4801);
        b->setColour (juce::TextButton::buttonOnColourId, theme::accent());
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }
    clipTab.setToggleState (true, juce::dontSendNotification);
    clipTab.onClick = [this] { audioClipEditorOpen = false; showDetail (false); };
    deviceTab.onClick = [this] { showDetail (true); };

    snapButton.setToggleState (true, juce::dontSendNotification);
    highlightButton.setToggleState (true, juce::dontSendNotification);
    previewButton.setToggleState (true, juce::dontSendNotification);
    audioClipEditor.onClose = [this]
    {
        audioClipEditorOpen = false;
        showDetail (false);
        resized();
    };
    audioClipEditor.onSourceBpmChanged = [this] (double bpm)
    {
        return project.setAudioClipSourceBpm (project.selectedTrackId, project.selectedAudioClipId, bpm);
    };

    // botões não roubam o foco do teclado (Espaço continua sendo play/pause)
    for (auto* b : { &scaleButton, &highlightButton, &foldButton, &foldScaleButton, &drawButton, &snapButton, &previewButton,
                     &zoomHMinus, &zoomHPlus, &zoomVMinus, &zoomVPlus, &clipTab, &deviceTab })
        b->setMouseClickGrabsKeyboardFocus (false);
    rootBox.setMouseClickGrabsKeyboardFocus (false);
    modeBox.setMouseClickGrabsKeyboardFocus (false);
    lengthBox.setMouseClickGrabsKeyboardFocus (false);

    rootBox.onChange = [this] { applyScale(); };
    modeBox.onChange = [this] { applyScale(); };
    lengthBox.onChange = [this] { applyLength(); };
    scaleButton.onClick = [this] { model.scaleOn = scaleButton.getToggleState(); model.rebuildRows(); model.sendChangeMessage(); };
    highlightButton.onClick = [this] { model.highlight = highlightButton.getToggleState(); model.sendChangeMessage(); };
    foldButton.onClick = [this] { model.fold = foldButton.getToggleState(); model.rebuildRows(); model.sendChangeMessage(); };
    foldScaleButton.onClick = [this] { model.foldScale = foldScaleButton.getToggleState(); model.rebuildRows(); model.sendChangeMessage(); };
    drawButton.onClick = [this] { model.drawMode = drawButton.getToggleState(); };
    snapButton.onClick = [this] { model.snap = snapButton.getToggleState(); model.sendChangeMessage(); };
    previewButton.onClick = [this] { model.previewOn = previewButton.getToggleState(); };
    zoomHMinus.onClick = [this] { editor.zoomHorizontal (1.0 / 1.25); };
    zoomHPlus.onClick  = [this] { editor.zoomHorizontal (1.25); };
    zoomVMinus.onClick = [this] { editor.zoomVertical (-2); };
    zoomVPlus.onClick  = [this] { editor.zoomVertical (2); };

    // ---- projeto <-> engine ----
    project.onTrackAdded = [this] (int id)
    {
        const auto* track = project.track (id);
        const auto type = track != nullptr ? track->type : roll::TrackType::Midi;
        engine.addTrack (id, type);
        if (type == roll::TrackType::Midi)
            engine.publishNotes (id, {});
        applyMixers();
    };
    project.onTrackRemoved = [this] (int id)
    {
        closeWindowsOfTrack (id);
        engine.removeTrack (id);
        publishAudioTrack (id);
    };
    project.onTrackNotes = [this] (int id)
    {
        if (auto* t = project.track (id); t != nullptr && t->type == roll::TrackType::Midi)
            engine.publishNotes (id, roll::flatten (*t));
    };
    project.onAudioClipsChanged = [this] (int id) { publishAudioTrack (id); };
    project.onMixerChanged = [this] { applyMixers(); };
    project.onSelectionChanged = [this] { onSelectionChanged(); };
    project.onLoopChanged = [this] { engine.graph().setLoopRegion (project.loopStart, project.loopEnd); };
    project.addChangeListener (this);
    model.addChangeListener (this);

    // ---- piano roll <-> projeto ----
    model.onNotesChanged = [this] { project.notifyTrackNotes (project.selectedTrackId); project.sendChangeMessage(); };
    model.onLengthChanged = [this]
    {
        project.notifyTrackNotes (project.selectedTrackId);
        project.sendChangeMessage();
    };
    model.onPreview = [this] (int pitch, bool on)
    {
        if (! on)                 engine.previewNote (project.selectedTrackId, pitch, false);
        else if (model.previewOn) engine.previewNote (project.selectedTrackId, pitch, true);
    };
    model.onInsertMarker = [this] (double local)
    {
        if (! engine.graph().isPlaying())
        {
            const double g = model.clipStartBeats + local;
            transportPosition = g;
            engine.graph().seek (g);
            arrangement.setPlayhead (g);
            editor.setPlayhead (local);
        }
    };
    editor.onSeek = [this] (double local)
    {
        const double g = model.clipStartBeats + local;
        transportPosition = g;
        engine.graph().seek (g);
        arrangement.setPlayhead (g);
        editor.setPlayhead (local);
    };

    arrangement.onSeek = [this] (double g)
    {
        transportPosition = g;
        engine.graph().seek (g);
        arrangement.setPlayhead (g);
        editor.setPlayhead (g - model.clipStartBeats);
    };
    arrangement.meterProvider = [this] (int id) { return engine.peak (id); };
    arrangement.deviceSummary = [this] (int id) { return deviceSummary (id); };
    arrangement.canAcceptPluginDrop = [this] (const juce::String& identifier, int trackId)
    {
        const auto description = scanner.list().getTypeForFile (identifier);
        return description != nullptr
            && (trackId != AudioEngine::masterTrackId || engine.canAcceptPlugin (trackId, *description));
    };
    arrangement.onPluginDropped = [this] (const juce::String& identifier, int trackId, double)
    {
        if (auto description = scanner.list().getTypeForFile (identifier))
        {
            auto* target = project.track (trackId);
            if (trackId == AudioEngine::masterTrackId && ! engine.canAcceptPlugin (trackId, *description))
            {
                browser.setStatus ("A trilha Master aceita apenas efeitos.");
                return;
            }
            if (! engine.canAcceptPlugin (trackId, *description))
            {
                const auto compatible = std::find_if (project.tracks.begin(), project.tracks.end(),
                    [this, &description] (const auto& track)
                    {
                        return engine.canAcceptPlugin (track->id, *description);
                    });
                target = compatible != project.tracks.end() ? compatible->get() : nullptr;
            }
            if (target == nullptr)
                target = project.addTrack (roll::TrackType::Midi);
            project.selectTrack (target->id);
            loadDevice (*description);
        }
    };
    arrangement.canAcceptAudioDrop = [] (const juce::File& file, int trackId)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        return trackId != AudioEngine::masterTrackId && file.existsAsFile() && (extension == ".wav" || extension == ".wave"
            || extension == ".aif" || extension == ".aiff" || extension == ".flac" || extension == ".ogg");
    };
    arrangement.onAudioDropped = [this] (const juce::File& file, int trackId, double beat)
    {
        auto* target = project.track (trackId);
        if (target == nullptr || target->type != roll::TrackType::Audio)
        {
            const auto existing = std::find_if (project.tracks.begin(), project.tracks.end(),
                [] (const auto& candidate) { return candidate->type == roll::TrackType::Audio; });
            target = existing != project.tracks.end() ? existing->get() : project.addTrack (roll::TrackType::Audio);
        }
        loadAudioFile (file, target->id, beat);
    };
    arrangement.onAudioClipDoubleClicked = [this] (int trackId, int clipId)
    {
        openAudioClipEditor (trackId, clipId);
    };
    arrangement.onConsolidateRequested = [this] (double startBeat, double endBeat, int firstTrack, int lastTrack)
    {
        consolidateAudioRange (startBeat, endBeat, firstTrack, lastTrack);
    };
    audioBrowser.canDeleteFile = [this] (const juce::File& file)
    {
        for (const auto& track : project.tracks)
            for (const auto& clip : track->audioClips)
                if (juce::File (juce::String (clip->filePath)) == file)
                    return false;
        return true;
    };
    arrangement.onGridChanged = [this] { refreshAudioClipEditor(); };
    audioBrowser.onPreviewRequested = [this] (const juce::File& file)
    {
        const int request = ++previewLoadToken;
        const std::weak_ptr<std::atomic<bool>> token = lifetimeToken;
        engine.loadAudioFileAsync (file, [this, token, file, request]
            (const juce::String& error, std::shared_ptr<const AudioFileData> data)
        {
            const auto alive = token.lock();
            if (alive == nullptr || ! alive->load() || request != previewLoadToken)
                return;
            if (error.isNotEmpty() || data == nullptr)
            {
                audioBrowser.setStatus ("Erro no preview: " + error);
                return;
            }
            engine.previewAudio (std::move (data));
            audioBrowser.setStatus ("Tocando: " + file.getFileName());
        });
    };
    audioBrowser.onPreviewStopRequested = [this]
    {
        ++previewLoadToken;
        engine.stopAudioPreview();
    };
    audioBrowser.onPreviewGainChanged = [this] (float gain) { engine.setAudioPreviewGain (gain); };
    transport.onPosition = [this] (double beats)
    {
        transportPosition = beats;
        const double bpm = engine.graph().getBpm();
        if (std::abs (bpm - displayedBpm) > 1.0e-6)
        {
            displayedBpm = bpm;
            refreshAudioClipEditor();
        }
        arrangement.setPlayhead (beats);
        editor.setPlayhead (beats - model.clipStartBeats);
    };
    transport.onLoopToggled = [this] (bool on) { arrangement.setLoopEnabled (on); };
    arrangement.onLoopToggleRequested = [this] (bool on) { transport.setLoopEnabled (on); };
    transport.onBrowserToggled = [this] (bool visible) { setBrowserVisible (visible); };
    transport.onFileBrowserToggled = [this] (bool visible) { setAudioBrowserVisible (visible); };
    transport.onFavoritesToggled = [this] (bool visible) { setFavoritesVisible (visible); };
    transport.onPianoRollToggled = [this] (bool visible) { setPianoRollVisible (visible); };

    deviceView.onOpen = [this] (int trackId, int deviceId) { openDeviceEditor (trackId, deviceId); };
    deviceView.onBypass = [this] (int trackId, int deviceId, bool bypassed)
    {
        engine.setDeviceBypassed (trackId, deviceId, bypassed);
    };
    deviceView.onRemove = [this] (int trackId, int deviceId)
    {
        windows.erase (deviceId);
        engine.removeDevice (trackId, deviceId);
        refreshDeviceView();
        project.sendChangeMessage();
    };

    browser.onLoadRequested = [this] (const juce::PluginDescription& d) { loadDevice (d); };
    scanner.onProgress = [this] (const juce::String& n)
    {
        scanningPlugins = true;
        scanMessage = "Verificando: " + juce::File (n).getFileName();
        browser.setStatus (scanMessage);
        updateActionIndicator();
    };
    scanner.onFinished = [this]
    {
        scanningPlugins = false;
        scanMessage.clear();
        browser.refresh();
        browser.setStatus (juce::String (scanner.list().getNumTypes()) + " plugins encontrados");
        updateActionIndicator();
    };

    engine.graph().setLoopRegion (project.loopStart, project.loopEnd);
    if (project.tracks.empty())
    {
        auto* midiTrack = project.addTrack();
        project.addTrack (roll::TrackType::Audio);
        project.selectTrack (midiTrack->id);
    }
    onSelectionChanged();
    changeListenerCallback (&model);
    showDetail (false);

    setWantsKeyboardFocus (true);
    setSize (1360, 800);
    startTimerHz (30);

    scanningPlugins = true;
    scanMessage = "Procurando plugins...";
    browser.setStatus ("Procurando plugins...");
    updateActionIndicator();
    scanner.startScan();
}

MainComponent::~MainComponent()
{
    lifetimeToken->store (false);
    stopTimer();
    project.removeChangeListener (this);
    model.removeChangeListener (this);
    windows.clear();                              // fecha as UIs dos plugins antes do engine
}

// ------------------------------------------------------------ layout

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background());
    g.setColour (theme::border());
    g.fillRoundedRectangle (splitterRect.toFloat().reduced (static_cast<float> (splitterRect.getWidth()) * 0.45f, 2.0f), 2.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    transport.setBounds (area.removeFromTop (44));
    if (actionIndicator != nullptr)
        actionIndicator->setBounds ((getWidth() - 280) / 2, 50, 280, 44);

    auto right = area.reduced (8);
    if (browser.isVisible() || audioBrowser.isVisible())
    {
        const int maxBrowserWidth = std::max (200, std::min (460, right.getWidth() - 760));
        browserWidth = std::clamp (browserWidth, 200, maxBrowserWidth);
        const auto bounds = right.removeFromLeft (browserWidth);
        browser.setBounds (browser.isVisible() ? bounds : juce::Rectangle<int>());
        audioBrowser.setBounds (audioBrowser.isVisible() ? bounds : juce::Rectangle<int>());
        browserSplitterRect = right.removeFromLeft (8);
    }
    else
    {
        browser.setBounds ({});
        audioBrowser.setBounds ({});
        browserSplitterRect = {};
    }

    rightTop = right.getY();
    rightHeight = std::max (1, right.getHeight());

    if (! pianoRollVisible)
    {
        arrangement.setBounds (right);
        splitterRect = {};
        clipTab.setBounds ({});
        deviceTab.setBounds ({});
        deviceView.setBounds ({});
        audioClipEditor.setBounds ({});
        for (auto* c : clipControls) c->setBounds ({});
        return;
    }

    const int arrH = std::clamp (static_cast<int> (right.getHeight() * arrangementRatio), 130, std::max (131, right.getHeight() - 230));
    arrangement.setBounds (right.removeFromTop (arrH));
    splitterRect = right.removeFromTop (10);

    auto tabs = right.removeFromTop (26);
    clipTab.setBounds (tabs.removeFromLeft (80));
    tabs.removeFromLeft (4);
    deviceTab.setBounds (tabs.removeFromLeft (110));
    right.removeFromTop (4);

    deviceView.setBounds (right);
    audioClipEditor.setBounds (right);

    auto side = right.removeFromLeft (170);
    right.removeFromLeft (8);
    editor.setBounds (right);

    scaleCaption.setBounds (side.removeFromTop (16));
    auto row = side.removeFromTop (26);
    rootBox.setBounds (row.removeFromLeft (56));
    row.removeFromLeft (4);
    modeBox.setBounds (row);
    side.removeFromTop (4);
    row = side.removeFromTop (26);
    scaleButton.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (4);
    highlightButton.setBounds (row);
    side.removeFromTop (4);
    row = side.removeFromTop (26);
    foldButton.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (4);
    foldScaleButton.setBounds (row);
    side.removeFromTop (8);
    lengthCaption.setBounds (side.removeFromTop (16));
    lengthBox.setBounds (side.removeFromTop (26));
    side.removeFromTop (8);
    gridCaption.setBounds (side.removeFromTop (16));
    gridLabel.setBounds (side.removeFromTop (20));
    row = side.removeFromTop (26);
    drawButton.setBounds (row.removeFromLeft (84));
    row.removeFromLeft (4);
    snapButton.setBounds (row);
    side.removeFromTop (4);
    previewButton.setBounds (side.removeFromTop (26).removeFromLeft (84));
    side.removeFromTop (8);
    zoomCaption.setBounds (side.removeFromTop (16));
    row = side.removeFromTop (26);
    zoomHMinus.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (4);
    zoomHPlus.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (8);
    zoomVMinus.setBounds (row.removeFromLeft (40));
    row.removeFromLeft (4);
    zoomVPlus.setBounds (row.removeFromLeft (40));
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    draggingBrowserSplitter = browserSplitterRect.expanded (4, 0).contains (e.getPosition());
    draggingSplitter = splitterRect.expanded (0, 4).contains (e.getPosition());
    grabKeyboardFocus();
}

void MainComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBrowserSplitter)
    {
        const int maxBrowserWidth = std::max (200, std::min (460, getWidth() - 776));
        browserWidth = std::clamp (e.x - 8, 200, maxBrowserWidth);
        resized();
        repaint();
    }
    else if (draggingSplitter)
    {
        arrangementRatio = std::clamp (static_cast<double> (e.y - rightTop) / rightHeight, 0.15, 0.75);
        resized();
        repaint();
    }
}

void MainComponent::mouseMove (const juce::MouseEvent& e)
{
    if (browserSplitterRect.expanded (4, 0).contains (e.getPosition()))
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else if (splitterRect.expanded (0, 4).contains (e.getPosition()))
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void MainComponent::showDetail (bool devices)
{
    showingDevices = devices;
    clipTab.setToggleState (! devices, juce::dontSendNotification);
    deviceTab.setToggleState (devices, juce::dontSendNotification);
    for (auto* c : clipControls) c->setVisible (! devices && ! audioClipEditorOpen);
    deviceView.setVisible (devices);
    audioClipEditor.setVisible (! devices && audioClipEditorOpen);
    if (devices) refreshDeviceView();
}

void MainComponent::setBrowserVisible (bool visible)
{
    if (visible && audioBrowser.isVisible())
    {
        audioBrowser.setVisible (false);
        audioBrowser.setFavoritesMode (false);
        transport.setFileBrowserVisible (false);
        transport.setFavoritesVisible (false);
    }
    browser.setVisible (visible);
    transport.setBrowserVisible (visible);
    resized();
}

void MainComponent::setAudioBrowserVisible (bool visible)
{
    if (visible && browser.isVisible())
    {
        browser.setVisible (false);
        transport.setBrowserVisible (false);
    }
    if (visible)
        audioBrowser.setFavoritesMode (false);
    audioBrowser.setVisible (visible);
    transport.setFileBrowserVisible (visible);
    transport.setFavoritesVisible (false);
    resized();
}

void MainComponent::setFavoritesVisible (bool visible)
{
    if (visible && browser.isVisible())
    {
        browser.setVisible (false);
        transport.setBrowserVisible (false);
    }
    audioBrowser.setFavoritesMode (visible);
    audioBrowser.setVisible (visible);
    transport.setFileBrowserVisible (false);
    transport.setFavoritesVisible (visible);
    resized();
}

void MainComponent::setPianoRollVisible (bool visible)
{
    pianoRollVisible = visible;
    transport.setPianoRollVisible (visible);
    resized();
}

void MainComponent::loadAudioFile (const juce::File& file, int trackId, double startBeat)
{
    ++pendingAudioLoads;
    audioBrowser.setStatus ("Carregando " + file.getFileName() + "...");
    updateActionIndicator();
    const std::weak_ptr<std::atomic<bool>> token = lifetimeToken;
    engine.loadAudioFileAsync (file, [this, token, file, trackId, startBeat]
        (const juce::String& error, std::shared_ptr<const AudioFileData> data)
    {
        const auto alive = token.lock();
        if (alive == nullptr || ! alive->load()) return;

        pendingAudioLoads = std::max (0, pendingAudioLoads - 1);
        updateActionIndicator();
        if (error.isNotEmpty() || data == nullptr)
        {
            audioBrowser.setStatus ("Erro ao carregar audio: " + error);
            return;
        }

        auto* track = project.track (trackId);
        if (track == nullptr || track->type != roll::TrackType::Audio)
        {
            audioBrowser.setStatus ("A trilha de audio foi removida durante o carregamento.");
            return;
        }

        const auto path = file.getFullPathName().toStdString();
        audioAssets[path] = data;
        const double sourceBpm = sourceBpmFromFileName (file, engine.graph().getBpm());
        const double lengthBeats = data->durationSeconds * sourceBpm / 60.0;
        if (project.addAudioClip (trackId, startBeat, lengthBeats, path,
                                  file.getFileName().toStdString(), data->waveform,
                                  data->durationSeconds, sourceBpm) == nullptr)
        {
            publishAudioTrack (trackId);
            audioBrowser.setStatus ("Nao foi possivel inserir o audio nesse ponto da trilha.");
            return;
        }

        audioBrowser.setStatus (file.getFileName() + " adicionado");
    });
}

void MainComponent::consolidateAudioRange (double startBeat, double endBeat, int firstTrack, int lastTrack)
{
    if (firstTrack != lastTrack || firstTrack < 0 || firstTrack >= static_cast<int> (project.tracks.size())
        || startBeat < 0.0 || endBeat <= startBeat)
    {
        audioBrowser.setStatus ("Consolidar: selecione um trecho de uma unica faixa de audio.");
        return;
    }

    auto* track = project.tracks[static_cast<std::size_t> (firstTrack)].get();
    if (track->type != roll::TrackType::Audio)
    {
        audioBrowser.setStatus ("Consolidar: a faixa selecionada precisa ser de audio.");
        return;
    }

    constexpr double outputSampleRate = 44100.0;
    const double bpm = engine.graph().getBpm();
    if (! std::isfinite (bpm) || bpm <= 0.0)
    {
        audioBrowser.setStatus ("Consolidar: o BPM do projeto e invalido.");
        return;
    }
    const double secondsPerBeat = 60.0 / bpm;
    const double durationSeconds = (endBeat - startBeat) * secondsPerBeat;
    const auto frameCountDouble = std::ceil (durationSeconds * outputSampleRate);
    if (! std::isfinite (frameCountDouble) || frameCountDouble < 1.0
        || durationSeconds > 600.0 || frameCountDouble > static_cast<double> (std::numeric_limits<int>::max()))
    {
        audioBrowser.setStatus ("Consolidar: a selecao e grande demais para renderizar (maximo de 10 minutos).");
        return;
    }

    auto renderedSamples = std::make_shared<juce::AudioBuffer<float>> (2, static_cast<int> (frameCountDouble));
    renderedSamples->clear();
    bool foundSource = false;
    for (const auto& clip : track->audioClips)
    {
        const double overlapStart = std::max (startBeat, clip->start);
        const double overlapEnd = std::min (endBeat, clip->end());
        if (overlapEnd <= overlapStart)
            continue;

        const auto asset = audioAssets.find (clip->filePath);
        if (asset == audioAssets.end() || asset->second == nullptr || asset->second->samples == nullptr
            || asset->second->sampleRate <= 0.0)
        {
            audioBrowser.setStatus ("Consolidar: nao foi possivel acessar a origem de " + juce::String (clip->name));
            return;
        }

        foundSource = true;
        const auto& source = *asset->second->samples;
        const int firstFrame = std::max (0, static_cast<int> (std::floor ((overlapStart - startBeat)
                                                                           * secondsPerBeat * outputSampleRate)));
        const int lastFrame = std::min (renderedSamples->getNumSamples(),
            static_cast<int> (std::ceil ((overlapEnd - startBeat) * secondsPerBeat * outputSampleRate)));
        for (int frame = firstFrame; frame < lastFrame; ++frame)
        {
            const double beat = startBeat + static_cast<double> (frame) / (secondsPerBeat * outputSampleRate);
            if (beat < clip->start || beat >= clip->end())
                continue;

            const double sourcePosition = (clip->sourceOffsetSeconds
                + (beat - clip->start) * secondsPerBeat) * asset->second->sampleRate;
            if (! std::isfinite (sourcePosition) || sourcePosition < 0.0
                || sourcePosition >= source.getNumSamples())
                continue;
            const int sourceIndex = static_cast<int> (sourcePosition);
            const int nextIndex = std::min (sourceIndex + 1, source.getNumSamples() - 1);
            const float fraction = static_cast<float> (sourcePosition - sourceIndex);
            double gain = 1.0;
            if (clip->fadeInBeats > 0.0)
                gain *= std::clamp ((beat - clip->start) / clip->fadeInBeats, 0.0, 1.0);
            if (clip->fadeOutBeats > 0.0)
                gain *= std::clamp ((clip->end() - beat) / clip->fadeOutBeats, 0.0, 1.0);

            for (int channel = 0; channel < renderedSamples->getNumChannels(); ++channel)
            {
                const int sourceChannel = std::min (channel, source.getNumChannels() - 1);
                const float sample = source.getSample (sourceChannel, sourceIndex)
                    + (source.getSample (sourceChannel, nextIndex) - source.getSample (sourceChannel, sourceIndex)) * fraction;
                renderedSamples->addSample (channel, frame, sample * static_cast<float> (gain));
            }
        }
    }

    if (! foundSource)
    {
        audioBrowser.setStatus ("Consolidar: nao ha clips de audio na selecao.");
        return;
    }

    auto outputDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                              .getChildFile ("ProjectFreedom").getChildFile ("Consolidated");
    if (const auto error = outputDirectory.createDirectory(); error.failed())
    {
        audioBrowser.setStatus ("Consolidar: nao foi possivel criar a pasta de destino: " + error.getErrorMessage());
        return;
    }
    const auto outputFile = outputDirectory.getNonexistentChildFile (
        "Consolidated_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S"),
        ".wav", false);
    auto fileStream = std::make_unique<juce::FileOutputStream> (outputFile);
    if (! fileStream->openedOk())
    {
        audioBrowser.setStatus ("Consolidar: nao foi possivel criar o WAV.");
        return;
    }
    std::unique_ptr<juce::OutputStream> outputStream (std::move (fileStream));
    juce::WavAudioFormat wavFormat;
    const juce::AudioFormatWriterOptions writerOptions;
    auto writer = wavFormat.createWriterFor (outputStream,
        writerOptions.withSampleRate (outputSampleRate).withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (*renderedSamples, 0, renderedSamples->getNumSamples()))
    {
        writer.reset();
        outputStream.reset();
        outputFile.deleteFile();
        audioBrowser.setStatus ("Consolidar: falha ao gravar o arquivo WAV.");
        return;
    }
    writer.reset();

    auto data = std::make_shared<AudioFileData>();
    data->file = outputFile;
    data->samples = renderedSamples;
    data->sampleRate = outputSampleRate;
    data->durationSeconds = durationSeconds;
    constexpr int waveformBins = 512;
    data->waveform.resize (waveformBins, 0.0f);
    for (int bin = 0; bin < waveformBins; ++bin)
    {
        const int from = bin * renderedSamples->getNumSamples() / waveformBins;
        const int to = std::max (from + 1, (bin + 1) * renderedSamples->getNumSamples() / waveformBins);
        float peak = 0.0f;
        for (int frame = from; frame < std::min (to, renderedSamples->getNumSamples()); ++frame)
            for (int channel = 0; channel < renderedSamples->getNumChannels(); ++channel)
                peak = std::max (peak, std::abs (renderedSamples->getSample (channel, frame)));
        data->waveform[static_cast<std::size_t> (bin)] = peak;
    }

    const int trackId = track->id;
    std::vector<int> overlappingIds;
    for (const auto& clip : track->audioClips)
        if (clip->start < endBeat && clip->end() > startBeat)
            overlappingIds.push_back (clip->id);

    project.beginEdit();
    bool replaced = true;
    for (const int clipId : overlappingIds)
    {
        auto* clip = track->findAudioClip (clipId);
        if (clip == nullptr)
            continue;
        int innerClipId = clipId;
        if (clip->start < startBeat && clip->end() > startBeat)
        {
            innerClipId = project.splitAudioClip (trackId, clipId, startBeat);
            if (innerClipId < 0)
            {
                replaced = false;
                break;
            }
        }

        clip = track->findAudioClip (innerClipId);
        if (clip != nullptr && clip->end() > endBeat)
            project.splitAudioClip (trackId, innerClipId, endBeat);
        clip = track->findAudioClip (innerClipId);
        if (clip != nullptr && clip->start >= startBeat - 1.0e-9
            && clip->end() <= endBeat + 1.0e-9)
            project.deleteAudioClip (trackId, innerClipId);
    }

    const auto path = outputFile.getFullPathName().toStdString();
    audioAssets[path] = data;
    if (replaced && project.addAudioClip (trackId, startBeat, endBeat - startBeat, path,
                                          outputFile.getFileName().toStdString(), data->waveform,
                                          durationSeconds, bpm) == nullptr)
        replaced = false;
    project.endEdit();
    if (! replaced)
    {
        project.undo();
        audioAssets.erase (path);
        outputFile.deleteFile();
        audioBrowser.setStatus ("Consolidar: nao foi possivel substituir a selecao.");
        return;
    }

    audioBrowser.setStatus ("Selecao consolidada: " + outputFile.getFileName());
}

void MainComponent::publishAudioTrack (int trackId)
{
    std::vector<AudioClipPlayback> clips;
    if (const auto* track = project.track (trackId); track != nullptr && track->type == roll::TrackType::Audio)
        for (const auto& clip : track->audioClips)
            if (const auto asset = audioAssets.find (clip->filePath); asset != audioAssets.end())
                clips.push_back ({ clip->start, clip->lengthBeats, clip->sourceBpm,
                                   clip->sourceOffsetSeconds, clip->fadeInBeats,
                                   clip->fadeOutBeats, asset->second });
    engine.publishAudioClips (trackId, clips);

    for (auto it = audioAssets.begin(); it != audioAssets.end();)
        if (! project.retainsAudioFile (it->first))
            it = audioAssets.erase (it);
        else
            ++it;
}

bool MainComponent::selectedTrackSpan (double& start, double& end) const
{
    const auto* track = project.selectedTrack();
    if (track == nullptr)
        return false;

    start = std::numeric_limits<double>::max();
    end = 0.0;
    if (track->type == roll::TrackType::Audio)
    {
        for (const auto& clip : track->audioClips)
        {
            start = std::min (start, clip->start);
            end = std::max (end, clip->end());
        }
    }
    else
    {
        for (const auto& clip : track->clips)
        {
            start = std::min (start, clip->start);
            end = std::max (end, clip->end());
        }
    }
    return start != std::numeric_limits<double>::max();
}

void MainComponent::playSelectedTrack()
{
    if (engine.graph().isPlaying())
    {
        transport.togglePlay();
        return;
    }

    double start = 0.0, end = 0.0;
    const auto* selectedTrack = project.selectedTrack();
    const auto* selectedAudio = selectedTrack != nullptr && selectedTrack->type == roll::TrackType::Audio
        ? selectedTrack->findAudioClip (project.selectedAudioClipId) : nullptr;
    if (selectedAudio != nullptr)
    {
        start = selectedAudio->start;
        end = selectedAudio->end();
        if (transportPosition >= start && transportPosition < end)
            start = transportPosition;
    }

    if (selectedAudio != nullptr || selectedTrackSpan (start, end))
    {
        if (engine.graph().isLoop() && (start < project.loopStart || end > project.loopEnd))
            transport.setLoopEnabled (false);
        transportPosition = start;
        engine.graph().seek (start);
        arrangement.setPlayhead (start);
        editor.setPlayhead (start - model.clipStartBeats);
    }
    else
    {
        engine.graph().seek (0.0);
        arrangement.setPlayhead (0.0);
    }
    transport.togglePlay();
}

void MainComponent::loopSelectedTrack()
{
    double start = 0.0, end = 0.0;
    const auto* track = project.selectedTrack();
    const auto* audio = track != nullptr && track->type == roll::TrackType::Audio
        ? track->findAudioClip (project.selectedAudioClipId) : nullptr;
    const auto* midi = project.selectedClip();
    if (audio != nullptr)
    {
        start = audio->start;
        end = audio->end();
    }
    else if (midi != nullptr)
    {
        start = midi->start;
        end = midi->end();
    }
    else if (! selectedTrackSpan (start, end))
        return;

    project.setLoop (start, end);
    transport.setLoopEnabled (true);
}

// ------------------------------------------------------------ teclado

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();
    const auto keyCode = juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode()));
    if (ctrl && keyCode == 'Z' && ! k.getModifiers().isShiftDown())
        return project.undo();
    if (ctrl && (keyCode == 'Y' || (keyCode == 'Z' && k.getModifiers().isShiftDown())))
        return project.redo();

    if (k.getModifiers().isCtrlDown()
        && juce::CharacterFunctions::toLowerCase (k.getKeyCode()) == 'b')
    {
        setBrowserVisible (! browser.isVisible());
        return true;
    }
    if (k.getModifiers().isCtrlDown()
        && juce::CharacterFunctions::toLowerCase (k.getKeyCode()) == 'l')
    {
        loopSelectedTrack();
        return true;
    }
    if (k == juce::KeyPress::spaceKey) { playSelectedTrack(); return true; }
    if (k == juce::KeyPress::homeKey)  { transport.rewind(); return true; }
    if (k == juce::KeyPress (juce::KeyPress::F12Key) || k == juce::KeyPress (juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0))
    {
        showDetail (! showingDevices);
        return true;
    }
    if (k == juce::KeyPress (juce::KeyPress::F6Key)) { cycleFocus(); return true; }
    if (k.getModifiers().isCtrlDown() && k.getModifiers().isShiftDown())
    {
        const auto shiftedKey = juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode()));
        if (shiftedKey == 'T')
        {
            project.addTrack();
            return true;
        }
        if (shiftedKey == 'A')
        {
            project.addTrack (roll::TrackType::Audio);
            return true;
        }
    }
    return false;
}

void MainComponent::cycleFocus()
{
    // navegação por teclado: lista de plugins -> arranjo -> piano roll
    if (browser.hasKeyboardFocus (true))          arrangement.grabFocus();
    else if (arrangement.hasKeyboardFocus (true)) { if (showingDevices) showDetail (false); editor.grabFocus(); }
    else                                          browser.grabFocus();
}

// ------------------------------------------------------------ projeto / engine

juce::String MainComponent::deviceSummary (int trackId) const
{
    juce::StringArray names;
    for (const auto& d : engine.devices (trackId)) names.add (d.name);
    const auto summary = names.joinIntoString (" > ");
    if (trackId == 0) return summary.isNotEmpty() ? summary : theme::ui ("Efeitos Master");
    const auto* track = project.track (trackId);
    return summary.isNotEmpty() ? summary : (track != nullptr && track->type == roll::TrackType::Audio ? theme::ui ("Canal de audio") : juce::String());
}

void MainComponent::applyMixers()
{
    bool anySolo = false;
    for (const auto& t : project.tracks) anySolo |= t->solo;
    for (const auto& t : project.tracks)
    {
        engine.setTrackRouting (t->id, t->outputRouting);
        engine.setMixer (t->id, static_cast<float> (t->volume), static_cast<float> (t->pan), t->mute || (anySolo && ! t->solo));
    }
    if (auto* m = project.masterTrack.get())
        engine.setMasterMixer (static_cast<float> (m->volume), static_cast<float> (m->pan), m->mute);
}

void MainComponent::onSelectionChanged()
{
    auto* p = project.selectedClip();
    const auto* selectedTrack = project.selectedTrack();
    const auto* audio = selectedTrack != nullptr
        ? selectedTrack->findAudioClip (project.selectedAudioClipId) : nullptr;
    roll::Clip* target = p != nullptr ? &p->clip : nullptr;
    const bool changed = (target != (model.hasClip() ? &model.clip() : nullptr));

    model.clipStartBeats = p != nullptr ? p->start : 0.0;
    if (changed) model.setClip (target);

    if (p != nullptr)
        lengthBox.setSelectedId (static_cast<int> (std::lround (p->clip.length() / 4.0)), juce::dontSendNotification);
    lengthBox.setEnabled (p != nullptr);

    if (project.selectedAudioClipId < 0)
        audioClipEditorOpen = false;
    if (audio != nullptr)
    {
        transportPosition = audio->start;
        engine.graph().seek (transportPosition);
        arrangement.setPlayhead (transportPosition);
    }
    refreshAudioClipEditor();
    showDetail (showingDevices);
    refreshDeviceView();
}

void MainComponent::openAudioClipEditor (int trackId, int clipId)
{
    project.selectAudioClip (trackId, clipId);
    audioClipEditorOpen = true;
    refreshAudioClipEditor();
    showDetail (false);
    resized();
}

void MainComponent::refreshAudioClipEditor()
{
    const auto* track = project.selectedTrack();
    const auto* clip = track != nullptr ? track->findAudioClip (project.selectedAudioClipId) : nullptr;
    std::shared_ptr<const AudioFileData> source;
    if (clip != nullptr)
        if (const auto asset = audioAssets.find (clip->filePath); asset != audioAssets.end())
            source = asset->second;
    audioClipEditor.setClip (clip, std::move (source), engine.graph().getBpm(), arrangement.gridBeats());
}

void MainComponent::refreshDeviceView()
{
    if (auto* t = project.selectedTrack())
        deviceView.setDevices (t->id, juce::String (t->name), engine.devices (t->id));
}

void MainComponent::applyScale()
{
    model.scale = roll::Scale (rootBox.getSelectedId() - 1, static_cast<roll::Mode> (modeBox.getSelectedId() - 1));
    model.rebuildRows();
    model.sendChangeMessage();
}

void MainComponent::applyLength()
{
    auto* t = project.selectedTrack();
    auto* p = project.selectedClip();
    if (t == nullptr || p == nullptr) return;

    const double newLen = lengthBox.getSelectedId() * 4.0;
    if (! t->isFree (p->start, newLen, p->id))
    {
        lengthBox.setSelectedId (static_cast<int> (std::lround (p->clip.length() / 4.0)), juce::dontSendNotification);
        return;                                           // bateria em outro clip: não deixa
    }
    p->clip.setLength (newLen);
    project.growTo (p->end() + 64.0);
    project.notifyTrackNotes (t->id);
    project.sendChangeMessage();
    model.sendChangeMessage();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src == &model)
    {
        gridLabel.setText (model.gridLabel() + (model.drawMode ? "   (desenho)" : ""), juce::dontSendNotification);
        drawButton.setToggleState (model.drawMode, juce::dontSendNotification);
        snapButton.setToggleState (model.snap, juce::dontSendNotification);
        highlightButton.setToggleState (model.highlight, juce::dontSendNotification);
    }
    else if (src == &project)
    {
        refreshAudioClipEditor();
        model.clipStartBeats = project.selectedClip() != nullptr ? project.selectedClip()->start : 0.0;
        if (auto* p = project.selectedClip())
            lengthBox.setSelectedId (static_cast<int> (std::lround (p->clip.length() / 4.0)), juce::dontSendNotification);
        model.sendChangeMessage();                         // o clip pode ter mudado de tamanho
    }
}

void MainComponent::timerCallback()
{
    std::uint64_t lo, hi;
    engine.activeMask (project.selectedTrackId, lo, hi);
    if (lo != lastLo || hi != lastHi)                     // teclas acendem com as notas tocando
    {
        lastLo = lo; lastHi = hi;
        model.activeLo = lo; model.activeHi = hi;
        editor.repaintKeys();
    }
}

// ------------------------------------------------------------ dispositivos

void MainComponent::loadDevice (const juce::PluginDescription& desc)
{
    auto* t = project.selectedTrack();
    if (t == nullptr) return;
    const int trackId = t->id;
    if (! engine.canAcceptPlugin (trackId, desc))
    {
        browser.setStatus (trackId == 0 ? theme::ui ("A trilha Master aceita apenas efeitos.")
                                        : theme::ui ("Canais de audio aceitam apenas efeitos."));
        return;
    }

    if (desc.isInstrument)                                // o instrumento atual será substituído: fecha a UI dele
        for (const auto& d : engine.devices (trackId))
            if (d.isInstrument) windows.erase (d.id);

    const int loadToken = ++nextPluginLoadToken;
    pendingPluginLoads.emplace (loadToken, desc.name);
    updateActionIndicator();
    browser.setStatus ("Carregando " + desc.name + "...");
    engine.addDevice (trackId, desc, [this, trackId, loadToken] (const juce::String& error, int deviceId)
    {
        pendingPluginLoads.erase (loadToken);
        updateActionIndicator();
        if (error.isNotEmpty())
        {
            browser.setStatus ("Erro: " + error);
            return;
        }
        browser.setStatus (juce::String (scanner.list().getNumTypes()) + " plugins encontrados");
        refreshDeviceView();
        project.sendChangeMessage();
        openDeviceEditor (trackId, deviceId);
    });
}

void MainComponent::updateActionIndicator()
{
    if (actionIndicator == nullptr)
        return;

    if (pendingAudioLoads > 0)
    {
        const auto text = pendingAudioLoads == 1
            ? "Carregando audio..."
            : "Carregando " + juce::String (pendingAudioLoads) + " arquivos de audio...";
        actionIndicator->showMessage (text);
    }
    else if (! pendingPluginLoads.empty())
    {
        const auto text = pendingPluginLoads.size() == 1
            ? "Carregando " + pendingPluginLoads.begin()->second + "..."
            : "Carregando " + juce::String (static_cast<int> (pendingPluginLoads.size())) + " plugins...";
        actionIndicator->showMessage (text);
    }
    else if (scanningPlugins)
    {
        actionIndicator->showMessage (scanMessage.isNotEmpty() ? scanMessage : "Verificando plugins...");
    }
    else
    {
        actionIndicator->hide();
    }
}

void MainComponent::openDeviceEditor (int trackId, int deviceId)
{
    auto it = windows.find (deviceId);
    if (it != windows.end())
    {
        it->second->setMinimised (false);
        it->second->setVisible (true);
        it->second->toFront (true);
        return;
    }

    if (auto ed = engine.createDeviceEditor (trackId, deviceId))
    {
        juce::String title;
        for (const auto& d : engine.devices (trackId)) if (d.id == deviceId) title = d.name;
        if (auto* t = project.track (trackId)) title += "  -  " + juce::String (t->name);

        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* display = displays.getDisplayForRect (getScreenBounds());
        if (display == nullptr)
            display = displays.getPrimaryDisplay();

        const auto displayArea = display != nullptr ? display->userBounds.toNearestInt()
                                                    : juce::Rectangle<int> (0, 0, 1280, 720);
        windows[deviceId] = std::make_unique<PluginWindow> (title, std::move (ed), displayArea, nextPluginCascadeIndex++);
        windows[deviceId]->toFront (true);
    }
    else
    {
        juce::String pluginName;
        for (const auto& device : engine.devices (trackId))
            if (device.id == deviceId) { pluginName = device.name; break; }

        browser.setStatus ("Falha ao abrir interface de " + (pluginName.isNotEmpty() ? pluginName : "plugin") + ".");
    }
}

void MainComponent::closeWindowsOfTrack (int trackId)
{
    for (const auto& d : engine.devices (trackId)) windows.erase (d.id);
}
