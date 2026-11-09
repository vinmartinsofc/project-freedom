#include "AudioEngine.h"
#include <limits>

namespace
{
void decodeAudioFile (juce::AudioFormatManager& formats, const juce::File& file,
                      std::function<void (const juce::String&, std::shared_ptr<const AudioFileData>)> callback)
{
    auto reader = formats.createReaderFor (file);
    if (reader == nullptr)
    {
        juce::MessageManager::callAsync ([callback = std::move (callback)] { callback ("Formato de audio nao suportado.", {}); });
        return;
    }

    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > std::numeric_limits<int>::max()
        || reader->numChannels == 0 || reader->sampleRate <= 0.0)
    {
        juce::MessageManager::callAsync ([callback = std::move (callback)] { callback ("Arquivo de audio vazio ou grande demais.", {}); });
        return;
    }

    const int numSamples = static_cast<int> (reader->lengthInSamples);
    const int numChannels = static_cast<int> (std::min (2u, reader->numChannels));
    constexpr std::uint64_t maxDecodedBytes = 512ULL * 1024 * 1024;
    const auto decodedBytes = static_cast<std::uint64_t> (numSamples)
                            * static_cast<std::uint64_t> (numChannels) * sizeof (float);
    if (decodedBytes > maxDecodedBytes)
    {
        juce::MessageManager::callAsync ([callback = std::move (callback)]
        {
            callback ("Arquivo de audio excede o limite de 512 MB decodificados.", {});
        });
        return;
    }

    auto data = std::make_shared<AudioFileData>();
    data->file = file;
    data->sampleRate = reader->sampleRate;
    data->durationSeconds = static_cast<double> (numSamples) / reader->sampleRate;
    data->samples = std::make_shared<juce::AudioBuffer<float>> (numChannels, numSamples);
    if (! reader->read (data->samples.get(), 0, numSamples, 0, true, true))
    {
        juce::MessageManager::callAsync ([callback = std::move (callback)] { callback ("Falha ao decodificar o arquivo de audio.", {}); });
        return;
    }

    constexpr int waveformResolution = 256;
    data->waveform.resize (waveformResolution);
    for (int bin = 0; bin < waveformResolution; ++bin)
    {
        const int start = bin * numSamples / waveformResolution;
        const int end = std::max (start + 1, (bin + 1) * numSamples / waveformResolution);
        float peak = 0.0f;
        for (int channel = 0; channel < numChannels; ++channel)
            for (int sample = start; sample < std::min (end, numSamples); ++sample)
                peak = std::max (peak, std::abs (data->samples->getSample (channel, sample)));
        data->waveform[static_cast<std::size_t> (bin)] = peak;
    }

    juce::MessageManager::callAsync ([callback = std::move (callback), data]
    {
        callback ({}, data);
    });
}
}

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

// ======================================================= TrackAudioSource

TrackAudioSource::TrackAudioSource (const BlockShared& s)
    : juce::AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
      shared (s)
{
    pendingClips.reserve (256);
    localClips.reserve (256);
}

void TrackAudioSource::publishClips (const std::vector<AudioClipPlayback>& clips)
{
    const juce::SpinLock::ScopedLockType lock (clipsLock);
    pendingClips = clips;
}

void TrackAudioSource::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    if (! shared.playing || shared.plan.beatsPerSample <= 0.0)
        return;

    {
        const juce::SpinLock::ScopedTryLockType lock (clipsLock);
        if (lock.isLocked())
            std::swap (localClips, pendingClips);
    }

    const double outputRate = getSampleRate();
    if (outputRate <= 0.0)
        return;
    const double bpm = shared.plan.beatsPerSample * outputRate * 60.0;

    for (int segmentIndex = 0; segmentIndex < shared.plan.count; ++segmentIndex)
    {
        const auto& segment = shared.plan.seg[segmentIndex];
        if (segment.samples <= 0)
            continue;

        for (const auto& clip : localClips)
        {
            if (clip.source == nullptr || clip.source->samples == nullptr
                || clip.source->sampleRate <= 0.0 || clip.lengthBeats <= 0.0)
                continue;

            const double activeStart = std::max (segment.a, clip.startBeat);
            const double activeEnd = std::min (segment.b, clip.startBeat + clip.lengthBeats);
            if (activeEnd <= activeStart)
                continue;

            const auto& source = *clip.source->samples;
            const double sourceStep = clip.source->sampleRate / outputRate;
            const double sourceStart = (activeStart - clip.startBeat) * 60.0 / bpm * clip.source->sampleRate;
            const int outputStart = segment.offset + static_cast<int> (std::lround ((activeStart - segment.a) / shared.plan.beatsPerSample));
            const int outputEnd = std::min (buffer.getNumSamples(),
                                            segment.offset + segment.samples);
            const int frameCount = std::min (outputEnd - outputStart,
                static_cast<int> (std::ceil ((activeEnd - activeStart) / shared.plan.beatsPerSample)));

            for (int i = 0; i < frameCount; ++i)
            {
                const double sourcePosition = sourceStart + i * sourceStep;
                const int sourceIndex = static_cast<int> (sourcePosition);
                if (sourceIndex >= source.getNumSamples())
                    break;

                const int nextIndex = std::min (sourceIndex + 1, source.getNumSamples() - 1);
                const float fraction = static_cast<float> (sourcePosition - sourceIndex);
                for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                {
                    const int sourceChannel = std::min (channel, source.getNumChannels() - 1);
                    const float first = source.getSample (sourceChannel, sourceIndex);
                    const float next = source.getSample (sourceChannel, nextIndex);
                    buffer.addSample (channel, outputStart + i, first + (next - first) * fraction);
                }
            }
        }
    }
}

// ======================================================= AudioPreviewSource

AudioPreviewSource::AudioPreviewSource()
    : juce::AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true))
{
}

void AudioPreviewSource::play (std::shared_ptr<const AudioFileData> source)
{
    {
        const juce::SpinLock::ScopedLockType lock (sourceLock);
        pendingSource = std::move (source);
        ++pendingVersion;
    }
    playing.store (true);
}

void AudioPreviewSource::stop()
{
    playing.store (false);
    const juce::SpinLock::ScopedLockType lock (sourceLock);
    pendingSource.reset();
    ++pendingVersion;
}

void AudioPreviewSource::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    buffer.clear();
    {
        const juce::SpinLock::ScopedTryLockType lock (sourceLock);
        if (lock.isLocked() && pendingVersion != localVersion)
        {
            std::swap (localSource, pendingSource);
            localVersion = pendingVersion;
            positionSamples = 0.0;
        }
    }

    if (! playing.load() || localSource == nullptr || localSource->samples == nullptr
        || localSource->sampleRate <= 0.0 || getSampleRate() <= 0.0)
        return;

    const auto& samples = *localSource->samples;
    const double sourceStep = localSource->sampleRate / getSampleRate();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const int firstIndex = static_cast<int> (positionSamples);
        if (firstIndex >= samples.getNumSamples())
        {
            playing.store (false);
            break;
        }

        const int secondIndex = std::min (firstIndex + 1, samples.getNumSamples() - 1);
        const float fraction = static_cast<float> (positionSamples - firstIndex);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const int sourceChannel = std::min (channel, samples.getNumChannels() - 1);
            const float first = samples.getSample (sourceChannel, firstIndex);
            const float second = samples.getSample (sourceChannel, secondIndex);
            buffer.setSample (channel, i, first + (second - first) * fraction);
        }
        positionSamples += sourceStep;
    }
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
    audioFormatManager.registerBasicFormats();

    seqGraph.setPlayConfigDetails (0, 2, 44100.0, 512);
    outputNode = seqGraph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
                     juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    auto previewSource = std::make_unique<AudioPreviewSource>();
    previewSourcePtr = previewSource.get();
    previewNode = seqGraph.addNode (std::move (previewSource));
    for (int channel = 0; channel < 2; ++channel)
        seqGraph.addConnection ({ { previewNode->nodeID, channel }, { outputNode->nodeID, channel } });

    deviceManager.initialiseWithDefaultDevices (0, 2);
    player.setProcessor (&seqGraph);
    deviceManager.addAudioCallback (&player);
}

AudioEngine::~AudioEngine()
{
    audioLoadPool.removeAllJobs (true, -1);
    previewSourcePtr->stop();
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

void AudioEngine::addTrack (int trackId, roll::TrackType type)
{
    if (find (trackId) != nullptr) return;

    TrackNodes t;
    t.audioTrack = type == roll::TrackType::Audio;
    if (t.audioTrack)
    {
        auto src = std::make_unique<TrackAudioSource> (seqGraph.sharedBlock());
        t.audioSourcePtr = src.get();
        t.source = seqGraph.addNode (std::move (src));
    }
    else
    {
        auto src = std::make_unique<TrackSource> (seqGraph.sharedBlock());
        t.sourcePtr = src.get();
        t.source = seqGraph.addNode (std::move (src));
    }

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

// Cadeia: MIDI -> instrumento -> efeitos ou áudio -> efeitos -> mixer -> saída
void AudioEngine::rewire (TrackNodes& t)
{
    clearConnectionsOf (t.source->nodeID);
    clearConnectionsOf (t.mixer->nodeID);
    for (auto& d : t.devices) clearConnectionsOf (d.node->nodeID);

    using AG = juce::AudioProcessorGraph;
    const int midi = AG::midiChannelIndex;

    for (int ch = 0; ch < 2; ++ch)
        seqGraph.addConnection ({ { t.mixer->nodeID, ch }, { outputNode->nodeID, ch } });

    if (t.audioTrack)
    {
        if (t.devices.empty())
        {
            for (int ch = 0; ch < 2; ++ch)
                seqGraph.addConnection ({ { t.source->nodeID, ch }, { t.mixer->nodeID, ch } });
            return;
        }

        for (size_t i = 0; i < t.devices.size(); ++i)
        {
            auto* proc = t.devices[i].node->getProcessor();
            const int inputs = proc->getTotalNumInputChannels();
            if (i == 0)
            {
                for (int ch = 0; ch < 2 && inputs > 0; ++ch)
                    seqGraph.addConnection ({ { t.source->nodeID, ch }, { t.devices[i].node->nodeID, std::min (ch, inputs - 1) } });
            }
            else if (inputs > 0)
            {
                auto* previous = t.devices[i - 1].node->getProcessor();
                const int outputs = std::max (1, previous->getTotalNumOutputChannels());
                for (int ch = 0; ch < 2; ++ch)
                    seqGraph.addConnection ({ { t.devices[i - 1].node->nodeID, std::min (ch, outputs - 1) },
                                              { t.devices[i].node->nodeID, std::min (ch, inputs - 1) } });
            }

            if (i + 1 == t.devices.size())
            {
                const int outputs = std::max (1, proc->getTotalNumOutputChannels());
                for (int ch = 0; ch < 2; ++ch)
                    seqGraph.addConnection ({ { t.devices[i].node->nodeID, std::min (ch, outputs - 1) }, { t.mixer->nodeID, ch } });
            }
        }
        return;
    }

    if (t.devices.empty()) return;
    if (t.devices.front().instrument)
        seqGraph.addConnection ({ { t.source->nodeID, midi }, { t.devices.front().node->nodeID, midi } });

    for (size_t i = 0; i < t.devices.size(); ++i)
    {
        auto* proc = t.devices[i].node->getProcessor();
        const int outs = std::max (1, proc->getTotalNumOutputChannels());
        const bool last = (i + 1 == t.devices.size());
        if (! last)
        {
            auto* next = t.devices[i + 1].node->getProcessor();
            const int inputs = next->getTotalNumInputChannels();
            if (inputs > 0)
                for (int ch = 0; ch < 2; ++ch)
                    seqGraph.addConnection ({ { t.devices[i].node->nodeID, std::min (ch, outs - 1) },
                                              { t.devices[i + 1].node->nodeID, std::min (ch, inputs - 1) } });
        }
        else
        {
            for (int ch = 0; ch < 2; ++ch)
                seqGraph.addConnection ({ { t.devices[i].node->nodeID, std::min (ch, outs - 1) }, { t.mixer->nodeID, ch } });
        }
    }
}

void AudioEngine::publishNotes (int trackId, const std::vector<roll::Note>& notes)
{
    if (auto* t = find (trackId); t != nullptr && t->sourcePtr != nullptr) t->sourcePtr->publishNotes (notes);
}

void AudioEngine::publishAudioClips (int trackId, const std::vector<AudioClipPlayback>& clips)
{
    if (auto* t = find (trackId); t != nullptr && t->audioSourcePtr != nullptr)
        t->audioSourcePtr->publishClips (clips);
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
    if (auto* t = find (trackId); t != nullptr && t->sourcePtr != nullptr)
        t->sourcePtr->activeMask (lo, hi);
}

float AudioEngine::peak (int trackId) const
{
    auto* t = find (trackId);
    return t != nullptr ? t->mixerPtr->peak() : 0.0f;
}

void AudioEngine::loadAudioFileAsync (
    const juce::File& file,
    std::function<void (const juce::String&, std::shared_ptr<const AudioFileData>)> callback)
{
    audioLoadPool.addJob ([this, file, callback = std::move (callback)]
    {
        decodeAudioFile (audioFormatManager, file, callback);
    });
}

void AudioEngine::previewAudio (std::shared_ptr<const AudioFileData> source)
{
    previewSourcePtr->play (std::move (source));
}

void AudioEngine::stopAudioPreview()
{
    previewSourcePtr->stop();
}

bool AudioEngine::canAcceptPlugin (int trackId, const juce::PluginDescription& description) const
{
    const auto* t = find (trackId);
    return t != nullptr && (! t->audioTrack || ! description.isInstrument);
}

void AudioEngine::addDevice (int trackId, const juce::PluginDescription& desc, std::function<void (const juce::String&, int)> onDone)
{
    auto* track = find (trackId);
    if (track == nullptr) { onDone ("Trilha inexistente.", 0); return; }
    if (track->audioTrack && desc.isInstrument) { onDone ("Canais de audio aceitam apenas efeitos.", 0); return; }

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
        onDone ("Este plugin ja esta sendo carregado nesta trilha.", 0);
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
                onDone ("Nao foi possivel adicionar o plugin ao grafo de audio.", 0);
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
