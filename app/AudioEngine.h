#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Arrangement.h"
#include "Sequencer.h"
#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Dados do bloco atual, escritos pelo grafo e lidos pelas trilhas (mesmo thread de áudio).
struct BlockShared
{
    roll::BlockPlan plan;
    bool playing = false;
    bool flush = false;       // soltar todas as notas neste bloco (pausa, stop, seek)
};

// Fonte MIDI de UMA trilha: toca as notas dela e mistura o MIDI de pré-escuta.
class TrackSource : public juce::AudioProcessor
{
public:
    explicit TrackSource (const BlockShared& s);

    void publishNotes (const std::vector<roll::Note>& notes);
    void preview (int pitch, bool on);
    void activeMask (std::uint64_t& lo, std::uint64_t& hi) const { lo = maskLo; hi = maskHi; }

    const juce::String getName() const override { return "Track MIDI"; }
    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    static constexpr size_t maxNotes = 8192;

private:
    const BlockShared& shared;
    roll::Sequencer sequencer;
    std::vector<roll::MidiEvent> events;
    std::vector<roll::Note> localNotes, pendingNotes;
    juce::SpinLock notesLock;
    unsigned notesVersion = 0, localVersion = 0;
    juce::MidiMessageCollector previewCollector;
    juce::MidiBuffer previewBuffer;
    std::atomic<std::uint64_t> maskLo { 0 }, maskHi { 0 };
};

// Volume / pan / mute da trilha (estéreo).
class TrackMixer : public juce::AudioProcessor
{
public:
    TrackMixer();
    void set (float gain, float pan, bool muted) { targetGain = gain; targetPan = pan; mute = muted; }
    float peak() const { return peakLevel; }

    const juce::String getName() const override { return "Track Mixer"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    std::atomic<float> targetGain { 0.8f }, targetPan { 0.0f }, peakLevel { 0.0f };
    std::atomic<bool> mute { false };
    float currentGain = 0.8f;
};

// Grafo principal: dono do relógio do transporte.
class SequencerGraph : public juce::AudioProcessorGraph
{
public:
    // ---- message thread ----
    void setPlaying (bool p)       { playing = p; }
    bool isPlaying() const         { return playing; }
    void rewind()                  { rewindRequested = true; }
    void seek (double beats)       { seekTarget = beats; seekRequested = true; }
    void setBpm (double b)         { bpm = b; }
    double getBpm() const          { return bpm; }
    void setLoop (bool l)          { loopEnabled = l; }
    bool isLoop() const            { return loopEnabled; }
    void setLoopRegion (double s, double e) { loopStart = s; loopEnd = e; }
    double positionBeats() const   { return positionAtomic; }
    const BlockShared& sharedBlock() const { return shared; }

    // ---- áudio thread ----
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessorGraph::processBlock;

private:
    roll::TransportClock clock;
    BlockShared shared;
    bool wasPlaying = false;
    std::atomic<bool>   playing { false }, rewindRequested { false }, seekRequested { false }, loopEnabled { true };
    std::atomic<double> bpm { 120.0 }, loopStart { 0.0 }, loopEnd { 16.0 }, seekTarget { 0.0 }, positionAtomic { 0.0 };
};

struct DeviceInfo
{
    int  id = 0;
    juce::String name;
    bool isInstrument = false;
    bool isBypassed = false;
};

struct AudioFileData
{
    juce::File file;
    std::shared_ptr<juce::AudioBuffer<float>> samples;
    std::vector<float> waveform;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;
};

struct AudioClipPlayback
{
    double startBeat = 0.0;
    double lengthBeats = 0.0;
    std::shared_ptr<const AudioFileData> source;
};

class TrackAudioSource : public juce::AudioProcessor
{
public:
    explicit TrackAudioSource (const BlockShared&);
    void publishClips (const std::vector<AudioClipPlayback>&);
    const juce::String getName() const override { return "Track Audio"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    const BlockShared& shared;
    juce::SpinLock clipsLock;
    std::vector<AudioClipPlayback> pendingClips, localClips;
};

class AudioPreviewSource : public juce::AudioProcessor
{
public:
    AudioPreviewSource();
    void play (std::shared_ptr<const AudioFileData>);
    void stop();
    const juce::String getName() const override { return "Audio Preview"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    juce::SpinLock sourceLock;
    std::shared_ptr<const AudioFileData> pendingSource, localSource;
    std::uint64_t pendingVersion = 0, localVersion = 0;
    std::atomic<bool> playing { false };
    double positionSamples = 0.0;
};

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    SequencerGraph& graph()                    { return seqGraph; }
    juce::AudioPluginFormatManager& formats()  { return formatManager; }

    // trilhas
    void addTrack (int trackId, roll::TrackType type = roll::TrackType::Midi);
    void removeTrack (int trackId);
    void publishNotes (int trackId, const std::vector<roll::Note>& notes);
    void publishAudioClips (int trackId, const std::vector<AudioClipPlayback>& clips);
    void setMixer (int trackId, float gain, float pan, bool muted);
    void previewNote (int trackId, int pitch, bool on);
    void activeMask (int trackId, std::uint64_t& lo, std::uint64_t& hi) const;
    float peak (int trackId) const;

    // dispositivos (plugins) da trilha: o instrumento vem sempre primeiro, efeitos em seguida
    void addDevice (int trackId, const juce::PluginDescription&, std::function<void (const juce::String& error, int deviceId)> onDone);
    void loadAudioFileAsync (const juce::File&, std::function<void (const juce::String&, std::shared_ptr<const AudioFileData>)>);
    void previewAudio (std::shared_ptr<const AudioFileData> source);
    void stopAudioPreview();
    bool canAcceptPlugin (int trackId, const juce::PluginDescription&) const;
    void removeDevice (int trackId, int deviceId);
    void setDeviceBypassed (int trackId, int deviceId, bool bypassed);
    std::vector<DeviceInfo> devices (int trackId) const;
    std::unique_ptr<juce::AudioProcessorEditor> createDeviceEditor (int trackId, int deviceId);

private:
    struct Device
    {
        int id;
        juce::AudioProcessorGraph::Node::Ptr node;
        juce::String name, pluginIdentifier;
        bool instrument;
    };
    struct TrackNodes
    {
        juce::AudioProcessorGraph::Node::Ptr source, mixer;
        TrackSource* sourcePtr = nullptr;
        TrackAudioSource* audioSourcePtr = nullptr;
        TrackMixer*  mixerPtr = nullptr;
        bool audioTrack = false;
        std::vector<Device> devices;
    };

    TrackNodes* find (int trackId);
    const TrackNodes* find (int trackId) const;
    void rewire (TrackNodes&);
    void clearConnectionsOf (juce::AudioProcessorGraph::NodeID);

    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::AudioFormatManager audioFormatManager;
    juce::ThreadPool audioLoadPool { 1 };
    SequencerGraph seqGraph;
    juce::AudioProcessorPlayer player;
    juce::AudioProcessorGraph::Node::Ptr outputNode;
    juce::AudioProcessorGraph::Node::Ptr previewNode;
    AudioPreviewSource* previewSourcePtr = nullptr;
    std::map<int, TrackNodes> tracks;
    std::set<std::pair<int, std::string>> pendingPluginLoads;
    int nextDeviceId = 1;
};
