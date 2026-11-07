#include "MainComponent.h"

// Janela de um plugin: minimizar + fechar (o tamanho do editor é do plugin).
class MainComponent::PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow (const juce::String& name, std::unique_ptr<juce::AudioProcessorEditor> editor, std::function<void()> onClose)
        : DocumentWindow (name, juce::Colours::darkgrey, DocumentWindow::minimiseButton | DocumentWindow::closeButton),
          closeCallback (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor.release(), true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { closeCallback(); }
private:
    std::function<void()> closeCallback;
};

MainComponent::MainComponent()
{
    addAndMakeVisible (transport);
    addAndMakeVisible (browser);
    addAndMakeVisible (arrangement);
    addAndMakeVisible (editor);
    addAndMakeVisible (deviceView);
    addAndMakeVisible (clipTab);
    addAndMakeVisible (deviceTab);

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
        engine.addTrack (id);
        engine.publishNotes (id, {});
        applyMixers();
    };
    project.onTrackRemoved = [this] (int id)
    {
        closeWindowsOfTrack (id);
        engine.removeTrack (id);
    };
    project.onTrackNotes = [this] (int id)
    {
        if (auto* t = project.track (id)) engine.publishNotes (id, roll::flatten (*t));
    };
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

    transport.onPosition = [this] (double beats)
    {
        arrangement.setPlayhead (beats);
        editor.setPlayhead (beats - model.clipStartBeats);
    };
    transport.onLoopToggled = [this] (bool on) { arrangement.setLoopEnabled (on); };

    deviceView.onOpen = [this] (int trackId, int deviceId) { openDeviceEditor (trackId, deviceId); };
    deviceView.onRemove = [this] (int trackId, int deviceId)
    {
        windows.erase (deviceId);
        engine.removeDevice (trackId, deviceId);
        refreshDeviceView();
        project.sendChangeMessage();
    };

    browser.onLoadRequested = [this] (const juce::PluginDescription& d) { loadDevice (d); };
    scanner.onProgress = [this] (const juce::String& n) { browser.setStatus ("Verificando: " + juce::File (n).getFileName()); };
    scanner.onFinished = [this]
    {
        browser.refresh();
        browser.setStatus (juce::String (scanner.list().getNumTypes()) + " plugins encontrados");
    };

    engine.graph().setLoopRegion (project.loopStart, project.loopEnd);
    project.addTrack();
    onSelectionChanged();
    changeListenerCallback (&model);
    showDetail (false);

    setWantsKeyboardFocus (true);
    setSize (1360, 800);
    startTimerHz (30);

    browser.setStatus ("Procurando plugins...");
    scanner.startScan();
}

MainComponent::~MainComponent()
{
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
    browser.setBounds (area.removeFromLeft (250));

    auto right = area.reduced (8);
    rightTop = right.getY();
    rightHeight = std::max (1, right.getHeight());

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
    draggingSplitter = splitterRect.expanded (0, 4).contains (e.getPosition());
    grabKeyboardFocus();
}

void MainComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingSplitter) return;
    arrangementRatio = std::clamp (static_cast<double> (e.y - rightTop) / rightHeight, 0.15, 0.75);
    resized();
}

void MainComponent::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (splitterRect.expanded (0, 4).contains (e.getPosition()) ? juce::MouseCursor::UpDownResizeCursor
                                                                             : juce::MouseCursor::NormalCursor);
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

// ------------------------------------------------------------ teclado

bool MainComponent::keyPressed (const juce::KeyPress& k)
{
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
    return names.joinIntoString (" > ");
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

    if (desc.isInstrument)                                // o instrumento atual será substituído: fecha a UI dele
        for (const auto& d : engine.devices (trackId))
            if (d.isInstrument) windows.erase (d.id);

    browser.setStatus ("Carregando " + desc.name + "...");
    const bool instrument = desc.isInstrument;
    engine.addDevice (trackId, desc, [this, trackId, instrument] (const juce::String& error, int deviceId)
    {
        if (error.isNotEmpty())
        {
            browser.setStatus ("Erro: " + error);
            return;
        }
        browser.setStatus (juce::String (scanner.list().getNumTypes()) + " plugins encontrados");
        refreshDeviceView();
        project.sendChangeMessage();
        if (instrument) openDeviceEditor (trackId, deviceId);
    });
}

void MainComponent::openDeviceEditor (int trackId, int deviceId)
{
    auto it = windows.find (deviceId);
    if (it != windows.end()) { it->second->toFront (true); return; }

    if (auto ed = engine.createDeviceEditor (trackId, deviceId))
    {
        juce::String title;
        for (const auto& d : engine.devices (trackId)) if (d.id == deviceId) title = d.name;
        if (auto* t = project.track (trackId)) title += "  -  " + juce::String (t->name);

        windows[deviceId] = std::make_unique<PluginWindow> (title, std::move (ed), [this, deviceId]
        {
            juce::MessageManager::callAsync ([this, deviceId] { windows.erase (deviceId); });
        });
    }
}

void MainComponent::closeWindowsOfTrack (int trackId)
{
    for (const auto& d : engine.devices (trackId)) windows.erase (d.id);
}
