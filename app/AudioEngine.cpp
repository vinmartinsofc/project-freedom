#include "AudioEngine.h"

// ======================================================= TrackSource

TrackSource::TrackSource (const BlockShared& s)
    : juce::AudioProcessor (BusesProperties()), shared (s)
{
    events.reserve (4096);
    localNotes.reserve (maxNotes);
    pendingNotes.reserve (maxNotes);
}

void TrackSource::prepareToPlay (double sampleRate, int)
{
    previewCollector.reset (sampleRate);
}

void TrackSource::publishNotes (const std::vector<roll::Note>& notes)
{
    const juce::SpinLock::ScopedLockType lock (notesLock);
    pendingNotes.assign (notes.begin(), notes.begin() + static_cast<std::ptrdiff_t> (std::min (notes.size(), maxNotes)));
    ++notesVersion;
}

void TrackSource::preview (int pitch, bool on)
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    previewCollector.addMessageToQueue (
        (on ? juce::MidiMessage::noteOn (1, pitch, 0.8f) : juce::MidiMessage::noteOff (1, pitch)).withTimeStamp (now));
}

void TrackSource::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    midi.clear();
    {
        const juce::SpinLock::ScopedTryLockType lock (notesLock);
        if (lock.isLocked() && notesVersion != localVersion)
        {
            localNotes.assign (pendingNotes.begin(), pendingNotes.end());
            localVersion = notesVersion;
        }
    }

    events.clear();
    if (shared.flush)   sequencer.flush (0, events);
    if (shared.playing) sequencer.process (localNotes, shared.plan, events);

    for (const auto& e : events)
        midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.pitch, static_cast<juce::uint8> (e.velocity))
                            : juce::MidiMessage::noteOff (1, e.pitch),
                       e.offset);

    previewBuffer.clear();
    previewCollector.removeNextBlockOfMessages (previewBuffer, buffer.getNumSamples());
    midi.addEvents (previewBuffer, 0, buffer.getNumSamples(), 0);

    std::uint64_t lo, hi;
    sequencer.activeMask (lo, hi);
    maskLo = lo; maskHi = hi;
}

// ======================================================= TrackMixer

TrackMixer::TrackMixer()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("In",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Out", juce::AudioChannelSet::stereo(), true)) {}

void TrackMixer::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int n = buffer.getNumSamples();
    const float target = mute.load() ? 0.0f : targetGain.load();
    const float pan = juce::jlimit (-1.0f, 1.0f, targetPan.load());
    const float gl = pan > 0.0f ? 1.0f - pan : 1.0f;
    const float gr = pan < 0.0f ? 1.0f + pan : 1.0f;

    const float start = currentGain;
    const float step = (target - start) / static_cast<float> (std::max (1, n));
    float peakNow = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        const float side = (ch == 0) ? gl : gr;
        float g = start;
        for (int i = 0; i < n; ++i)
        {
            g += step;
            data[i] *= g * side;
            peakNow = std::max (peakNow, std::abs (data[i]));
        }
    }
    currentGain = target;
    peakLevel = std::max (peakNow, peakLevel.load() * 0.92f);       // queda suave para o medidor
}

// ======================================================= SequencerGraph

void SequencerGraph::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    shared.flush = false;
    const bool nowPlaying = playing.load();

    if (wasPlaying && ! nowPlaying) shared.flush = true;                      // pausa: solta as notas
    wasPlaying = nowPlaying;

    if (rewindRequested.exchange (false)) { shared.flush = true; clock.setPosition (0.0); }
    if (seekRequested.exchange (false))   { shared.flush = true; clock.setPosition (seekTarget.load()); }

    shared.plan.count = 0;
    shared.plan.numSamples = buffer.getNumSamples();
    shared.playing = nowPlaying;

    if (nowPlaying)
    {
        roll::TransportSettings t;
        t.bpm = bpm.load();
        t.loop = loopEnabled.load();
        t.loopStart = loopStart.load();
        t.loopEnd = loopEnd.load();
        clock.plan (t, getSampleRate(), buffer.getNumSamples(), shared.plan);
    }

    positionAtomic = clock.position();
    juce::AudioProcessorGraph::processBlock (buffer, midi);
}

// ======================================================= AudioEngine

AudioEngine::AudioEngine()
{
    formatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());

    seqGraph.setPlayConfigDetails (0, 2, 44100.0, 512);
    outputNode = seqGraph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                     juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setProcessor (&seqGraph);
    deviceManager.addAudioCallback (&player);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
    seqGraph.clear();
}

AudioEngine::TrackNodes* AudioEngine::find (int trackId)
{
    auto it = tracks.find (trackId);
    return it == tracks.end() ? nullptr : &it->second;
}

const AudioEngine::TrackNodes* AudioEngine::find (int trackId) const
{
    auto it = tracks.find (trackId);
    return it == tracks.end() ? nullptr : &it->second;
}

void AudioEngine::addTrack (int trackId)
{
    if (find (trackId) != nullptr) return;

    TrackNodes t;
    auto src = std::make_unique<TrackSource> (seqGraph.sharedBlock());
    t.sourcePtr = src.get();
    t.source = seqGraph.addNode (std::move (src));

    auto mix = std::make_unique<TrackMixer>();
    t.mixerPtr = mix.get();
    t.mixer = seqGraph.addNode (std::move (mix));

    tracks[trackId] = std::move (t);
    rewire (tracks[trackId]);
}

void AudioEngine::removeTrack (int trackId)
{
    auto* t = find (trackId);
    if (t == nullptr) return;

    for (auto& d : t->devices) seqGraph.removeNode (d.node->nodeID);
    seqGraph.removeNode (t->source->nodeID);
    seqGraph.removeNode (t->mixer->nodeID);
    tracks.erase (trackId);
}

void AudioEngine::clearConnectionsOf (juce::AudioProcessorGraph::NodeID id)
{
    for (const auto& c : seqGraph.getConnections())
        if (c.source.nodeID == id || c.destination.nodeID == id)
            seqGraph.removeConnection (c);
}

// Cadeia: MIDI -> instrumento -> efeitos... -> mixer -> saída
void AudioEngine::rewire (TrackNodes& t)
{
    clearConnectionsOf (t.source->nodeID);
    clearConnectionsOf (t.mixer->nodeID);
    for (auto& d : t.devices) clearConnectionsOf (d.node->nodeID);

    using AG = juce::AudioProcessorGraph;
    const int midi = AG::midiChannelIndex;

    for (int ch = 0; ch < 2; ++ch)
        seqGraph.addConnection ({ { t.mixer->nodeID, ch }, { outputNode->nodeID, ch } });

    if (t.devices.empty()) return;

    if (t.devices.front().instrument)
        seqGraph.addConnection ({ { t.source->nodeID, midi }, { t.devices.front().node->nodeID, midi } });

    for (size_t i = 0; i < t.devices.size(); ++i)
    {
        auto* proc = t.devices[i].node->getProcessor();
        const int outs = std::max (1, proc->getTotalNumOutputChannels());
        const bool last = (i + 1 == t.devices.size());

        for (int ch = 0; ch < 2; ++ch)
        {
            const int srcCh = std::min (ch, outs - 1);
            if (last)
                seqGraph.addConnection ({ { t.devices[i].node->nodeID, srcCh }, { t.mixer->nodeID, ch } });
            else
            {
                auto* next = t.devices[i + 1].node->getProcessor();
                const int ins = next->getTotalNumInputChannels();
                if (ins > 0)
                    seqGraph.addConnection ({ { t.devices[i].node->nodeID, srcCh },
                                              { t.devices[i + 1].node->nodeID, std::min (ch, ins - 1) } });
            }
        }
    }
}

void AudioEngine::publishNotes (int trackId, const std::vector<roll::Note>& notes)
{
    if (auto* t = find (trackId)) t->sourcePtr->publishNotes (notes);
}

void AudioEngine::setMixer (int trackId, float gain, float pan, bool muted)
{
    if (auto* t = find (trackId)) t->mixerPtr->set (gain, pan, muted);
}

void AudioEngine::previewNote (int trackId, int pitch, bool on)
{
    if (auto* t = find (trackId)) t->sourcePtr->preview (pitch, on);
}

void AudioEngine::activeMask (int trackId, std::uint64_t& lo, std::uint64_t& hi) const
{
    lo = hi = 0;
    if (auto* t = find (trackId)) t->sourcePtr->activeMask (lo, hi);
}

float AudioEngine::peak (int trackId) const
{
    auto* t = find (trackId);
    return t != nullptr ? t->mixerPtr->peak() : 0.0f;
}

void AudioEngine::addDevice (int trackId, const juce::PluginDescription& desc, std::function<void (const juce::String&, int)> onDone)
{
    if (find (trackId) == nullptr) { onDone ("Trilha inexistente.", 0); return; }

    const auto pluginIdentifier = desc.createIdentifierString();
    for (const auto& device : find (trackId)->devices)
        if (device.pluginIdentifier == pluginIdentifier)
        {
            onDone ({}, device.id);
            return;
        }

    const auto pendingKey = std::make_pair (trackId, pluginIdentifier.toStdString());
    if (! pendingPluginLoads.insert (pendingKey).second)
    {
        onDone ("Este plugin já está sendo carregado nesta trilha.", 0);
        return;
    }

    double sampleRate = 44100.0;
    int blockSize = 512;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        sampleRate = dev->getCurrentSampleRate();
        blockSize  = dev->getCurrentBufferSizeSamples();
    }

    formatManager.createPluginInstanceAsync (desc, sampleRate, blockSize,
        [this, trackId, pluginIdentifier, pendingKey, onDone]
        (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            pendingPluginLoads.erase (pendingKey);
            auto* t = find (trackId);
            if (instance == nullptr || t == nullptr)
            {
                onDone (error.isNotEmpty() ? error : "Falha ao carregar o plugin.", 0);
                return;
            }

            Device d;
            d.id = nextDeviceId++;
            d.name = instance->getName();
            d.pluginIdentifier = pluginIdentifier;
            d.instrument = instance->getPluginDescription().isInstrument;
            d.node = seqGraph.addNode (std::move (instance));
            if (d.node == nullptr)
            {
                onDone (juce::String::fromUTF8 ("Não foi possível adicionar o plugin ao grafo de áudio."), 0);
                return;
            }

            if (d.instrument)
            {
                // um instrumento por trilha: o novo substitui o anterior
                if (! t->devices.empty() && t->devices.front().instrument)
                {
                    seqGraph.removeNode (t->devices.front().node->nodeID);
                    t->devices.erase (t->devices.begin());
                }
                t->devices.insert (t->devices.begin(), d);
            }
            else
                t->devices.push_back (d);

            rewire (*t);
            onDone ({}, d.id);
        });
}

void AudioEngine::removeDevice (int trackId, int deviceId)
{
    auto* t = find (trackId);
    if (t == nullptr) return;

    for (auto it = t->devices.begin(); it != t->devices.end(); ++it)
        if (it->id == deviceId)
        {
            seqGraph.removeNode (it->node->nodeID);
            t->devices.erase (it);
            break;
        }
    rewire (*t);
}

void AudioEngine::setDeviceBypassed (int trackId, int deviceId, bool bypassed)
{
    if (auto* t = find (trackId))
        for (auto& d : t->devices)
            if (d.id == deviceId)
            {
                d.node->setBypassed (bypassed);
                break;
            }
}

std::vector<DeviceInfo> AudioEngine::devices (int trackId) const
{
    std::vector<DeviceInfo> out;
    if (auto* t = find (trackId))
        for (const auto& d : t->devices)
            out.push_back ({ d.id, d.name, d.instrument, d.node->isBypassed() });
    return out;
}

std::unique_ptr<juce::AudioProcessorEditor> AudioEngine::createDeviceEditor (int trackId, int deviceId)
{
    if (auto* t = find (trackId))
        for (auto& d : t->devices)
            if (d.id == deviceId)
            {
                auto* proc = d.node->getProcessor();
                if (proc->hasEditor())
                    return std::unique_ptr<juce::AudioProcessorEditor> (proc->createEditorAndMakeActive());
            }
    return nullptr;
}
