#pragma once
#include <juce_events/juce_events.h>
#include "Arrangement.h"
#include <functional>
#include <memory>
#include <vector>

// Estado do projeto: trilhas, clips, seleção e região de loop. Vive na thread principal.
// O engine de áudio e as views ouvem os callbacks.
class ProjectModel : public juce::ChangeBroadcaster
{
public:
    ProjectModel();
    ~ProjectModel() override = default;

    std::unique_ptr<roll::Track> masterTrack;
    std::vector<std::unique_ptr<roll::Track>> tracks;
    int selectedTrackId = -1, selectedClipId = -1, selectedAudioClipId = -1;
    double loopStart = 0.0, loopEnd = 16.0;
    double totalBeats = 256.0;                       // 64 compassos visíveis (cresce com os clips)

    std::function<void (int trackId)> onTrackAdded, onTrackRemoved;   // removed: antes de apagar
    std::function<void (int trackId)> onTrackNotes;                   // notas da trilha mudaram
    std::function<void (int trackId)> onAudioClipsChanged;
    std::function<void()> onMixerChanged;
    std::function<void()> onSelectionChanged;                         // trilha/clip selecionados mudaram
    std::function<void()> onLoopChanged;

    roll::Track*     track (int id) const;
    roll::Track*     selectedTrack() const { return track (selectedTrackId); }
    roll::Placement* selectedClip() const;
    int  trackIndex (int id) const;

    roll::Track* addTrack (roll::TrackType type = roll::TrackType::Midi);
    void removeTrack (int id);
    void selectTrack (int id);
    void selectClip (int trackId, int clipId);

    roll::Placement* addClip (int trackId, double start, double length);
    int copyClip (int fromTrack, int clipId, int toTrack, double start);
    int duplicateClip (int trackId, int clipId);
    void deleteClip (int trackId, int clipId);
    int splitClip (int trackId, int clipId, double beat);
    roll::AudioPlacement* addAudioClip (int trackId, double start, double length,
                                        const std::string& filePath, const std::string& name,
                                        const std::vector<float>& waveform,
                                        double durationSeconds = 0.0, double sourceBpm = 120.0);
    void deleteAudioClip (int trackId, int clipId);
    int copyAudioClip (int fromTrack, int clipId, int toTrack, double start);
    int duplicateAudioClip (int trackId, int clipId);
    int splitAudioClip (int trackId, int clipId, double beat);
    bool resizeAudioClip (int trackId, int clipId, double length);
    bool setAudioClipFades (int trackId, int clipId, double fadeInBeats, double fadeOutBeats);
    bool setAudioClipSourceBpm (int trackId, int clipId, double sourceBpm);
    // move para outra trilha/posição; devolve o novo id do clip (ou -1 se não coube)
    int  moveClip (int fromTrack, int clipId, int toTrack, double newStart);
    int  moveAudioClip (int fromTrack, int clipId, int toTrack, double newStart);
    void selectAudioClip (int trackId, int clipId);
    void setTrackRouting (int trackId, roll::TrackOutputRouting routing);
    void notifyTrackNotes (int trackId);
    void notifyMixer()   { if (onMixerChanged) onMixerChanged(); sendChangeMessage(); }
    void setLoop (double s, double e);
    void growTo (double beats);
    void beginEdit();
    void endEdit();
    bool undo();
    bool redo();
    bool retainsAudioFile (const std::string& path) const;

private:
    struct Snapshot
    {
        std::vector<roll::Track> tracks;
        int selectedTrackId = -1, selectedClipId = -1, selectedAudioClipId = -1;
        int nextTrackId = 1;
        double loopStart = 0.0, loopEnd = 16.0, totalBeats = 256.0;
    };

    Snapshot makeSnapshot() const;
    void restoreSnapshot (Snapshot&&);
    void recordUndo();

    int nextTrackId = 1;
    std::vector<Snapshot> undoStack, redoStack;
    bool editInProgress = false;
};
