#include "MainComponent.h"
#include "WindowPlacement.h"
#include <algorithm>
#include <set>
#include <utility>

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
    clipTab.onClick = [this] { showDetail (false); };
    deviceTab.onClick = [this] { showDetail (true); };

    snapButton.setToggleState (true, juce::dontSendNotification);
    highlightButton.setToggleState (true, juce::dontSendNotification);
    previewButton.setToggleState (true, juce::dontSendNotification);

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
            engine.graph().seek (g);
            arrangement.setPlayhead (g);
            editor.setPlayhead (local);
        }
    };
    editor.onSeek = [this] (double local)
    {
        const double g = model.clipStartBeats + local;
        engine.graph().seek (g);
        arrangement.setPlayhead (g);
        editor.setPlayhead (local);
    };

    arrangement.onSeek = [this] (double g)
    {
        engine.graph().seek (g);
        arrangement.setPlayhead (g);
        editor.setPlayhead (g - model.clipStartBeats);
    };
    arrangement.meterProvider = [this] (int id) { return engine.peak (id); };
    arrangement.deviceSummary = [this] (int id) { return deviceSummary (id); };
    arrangement.canAcceptPluginDrop = [this] (const juce::String& identifier, int)
    {
        return scanner.list().getTypeForFile (identifier) != nullptr;
    };
    arrangement.onPluginDropped = [this] (const juce::String& identifier, int trackId, double)
    {
        if (auto description = scanner.list().getTypeForFile (identifier))
        {
            auto* target = project.track (trackId);
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
    arrangement.canAcceptAudioDrop = [] (const juce::File& file, int)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        return file.existsAsFile() && (extension == ".wav" || extension == ".wave"
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
    transport.onPosition = [this] (double beats)
    {
        arrangement.setPlayhead (beats);
        editor.setPlayhead (beats - model.clipStartBeats);
    };
    transport.onLoopToggled = [this] (bool on) { arrangement.setLoopEnabled (on); };
    transport.onBrowserToggled = [this] (bool visible) { setBrowserVisible (visible); };
    transport.onFileBrowserToggled = [this] (bool visible) { setAudioBrowserVisible (visible); };
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
    project.addTrack();
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
        const auto bounds = right.removeFromRight (browserWidth);
        browser.setBounds (browser.isVisible() ? bounds : juce::Rectangle<int>());
        audioBrowser.setBounds (audioBrowser.isVisible() ? bounds : juce::Rectangle<int>());
        browserSplitterRect = right.removeFromRight (8);
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
        browserWidth = std::clamp (getWidth() - 8 - e.x, 200, maxBrowserWidth);
        resized();
    }
    else if (draggingSplitter)
    {
        arrangementRatio = std::clamp (static_cast<double> (e.y - rightTop) / rightHeight, 0.15, 0.75);
        resized();
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
    for (auto* c : clipControls) c->setVisible (! devices);
    deviceView.setVisible (devices);
    if (devices) refreshDeviceView();
}

void MainComponent::setBrowserVisible (bool visible)
{
    if (visible && audioBrowser.isVisible())
    {
        audioBrowser.setVisible (false);
        transport.setFileBrowserVisible (false);
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
    audioBrowser.setVisible (visible);
    transport.setFileBrowserVisible (visible);
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
        const double lengthBeats = data->durationSeconds * engine.graph().getBpm() / 60.0;
        if (project.addAudioClip (trackId, startBeat, lengthBeats, path,
                                  file.getFileName().toStdString(), data->waveform) == nullptr)
        {
            publishAudioTrack (trackId);
            audioBrowser.setStatus ("Nao foi possivel inserir o audio nesse ponto da trilha.");
            return;
        }

        audioBrowser.setStatus (file.getFileName() + " adicionado");
    });
}

void MainComponent::publishAudioTrack (int trackId)
{
    std::vector<AudioClipPlayback> clips;
    if (const auto* track = project.track (trackId); track != nullptr && track->type == roll::TrackType::Audio)
        for (const auto& clip : track->audioClips)
            if (const auto asset = audioAssets.find (clip->filePath); asset != audioAssets.end())
                clips.push_back ({ clip->start, clip->lengthBeats, asset->second });
    engine.publishAudioClips (trackId, clips);

    std::set<std::string> activePaths;
    for (const auto& track : project.tracks)
        for (const auto& clip : track->audioClips)
            activePaths.insert (clip->filePath);
    for (auto it = audioAssets.begin(); it != audioAssets.end();)
        if (activePaths.count (it->first) == 0)
            it = audioAssets.erase (it);
        else
            ++it;
}

// ------------------------------------------------------------ teclado

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
    if (k.getModifiers().isCtrlDown()
        && juce::CharacterFunctions::toLowerCase (k.getKeyCode()) == 'b')
    {
        setBrowserVisible (! browser.isVisible());
        return true;
    }
    if (k == juce::KeyPress::spaceKey) { transport.togglePlay(); return true; }
    if (k == juce::KeyPress::homeKey)  { transport.rewind(); return true; }
    if (k == juce::KeyPress (juce::KeyPress::F12Key) || k == juce::KeyPress (juce::KeyPress::tabKey, juce::ModifierKeys::shiftModifier, 0))
    {
        showDetail (! showingDevices);
        return true;
    }
    if (k == juce::KeyPress (juce::KeyPress::F6Key)) { cycleFocus(); return true; }
    if (k.getModifiers().isCtrlDown() && k.getModifiers().isShiftDown()
        && juce::CharacterFunctions::toUpperCase (static_cast<juce::juce_wchar> (k.getKeyCode())) == 'T')
    {
        project.addTrack();
        return true;
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
    const auto* track = project.track (trackId);
    return summary.isNotEmpty() ? summary : (track != nullptr && track->type == roll::TrackType::Audio ? "Canal de audio" : juce::String());
}

void MainComponent::applyMixers()
{
    bool anySolo = false;
    for (const auto& t : project.tracks) anySolo |= t->solo;
    for (const auto& t : project.tracks)
        engine.setMixer (t->id, static_cast<float> (t->volume), static_cast<float> (t->pan), t->mute || (anySolo && ! t->solo));
}

void MainComponent::onSelectionChanged()
{
    auto* p = project.selectedClip();
    roll::Clip* target = p != nullptr ? &p->clip : nullptr;
    const bool changed = (target != (model.hasClip() ? &model.clip() : nullptr));

    model.clipStartBeats = p != nullptr ? p->start : 0.0;
    if (changed) model.setClip (target);

    if (p != nullptr)
        lengthBox.setSelectedId (static_cast<int> (std::lround (p->clip.length() / 4.0)), juce::dontSendNotification);
    lengthBox.setEnabled (p != nullptr);

    refreshDeviceView();
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
        browser.setStatus ("Canais de audio aceitam apenas efeitos.");
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
