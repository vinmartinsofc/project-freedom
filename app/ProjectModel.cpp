#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

ProjectModel::ProjectModel()
{
    masterTrack = std::make_unique<roll::Track>();
    masterTrack->id = 0;
    masterTrack->name = "Master";
    masterTrack->volume = 1.0;
    masterTrack->pan = 0.0;
}

ProjectModel::Snapshot ProjectModel::makeSnapshot() const
{
    Snapshot snapshot;
    snapshot.tracks.reserve (tracks.size());
    for (const auto& track : tracks)
        snapshot.tracks.push_back (*track);
    snapshot.selectedTrackId = selectedTrackId;
    snapshot.selectedClipId = selectedClipId;
    snapshot.selectedAudioClipId = selectedAudioClipId;
    snapshot.nextTrackId = nextTrackId;
    snapshot.loopStart = loopStart;
    snapshot.loopEnd = loopEnd;
    snapshot.totalBeats = totalBeats;
    return snapshot;
}

void ProjectModel::restoreSnapshot (Snapshot&& snapshot)
{
    tracks.clear();
    tracks.reserve (snapshot.tracks.size());
    for (const auto& track : snapshot.tracks)
        tracks.push_back (std::make_unique<roll::Track> (track));

    selectedTrackId = snapshot.selectedTrackId;
    selectedClipId = snapshot.selectedClipId;
    selectedAudioClipId = snapshot.selectedAudioClipId;
    nextTrackId = snapshot.nextTrackId;
    loopStart = snapshot.loopStart;
    loopEnd = snapshot.loopEnd;
    totalBeats = snapshot.totalBeats;

    for (const auto& track : tracks)
    {
        if (track->type == roll::TrackType::Audio)
        {
            if (onAudioClipsChanged) onAudioClipsChanged (track->id);
        }
        else if (onTrackNotes)
            onTrackNotes (track->id);
    }
    if (onSelectionChanged) onSelectionChanged();
    if (onMixerChanged) onMixerChanged();
    if (onLoopChanged) onLoopChanged();
    sendChangeMessage();
}

void ProjectModel::recordUndo()
{
    if (! editInProgress)
    {
        undoStack.push_back (makeSnapshot());
        if (undoStack.size() > 200)
            undoStack.erase (undoStack.begin());
    }
    redoStack.clear();
}

void ProjectModel::beginEdit()
{
    if (editInProgress)
        return;
    editInProgress = true;
    undoStack.push_back (makeSnapshot());
    if (undoStack.size() > 200)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void ProjectModel::endEdit()
{
    editInProgress = false;
}

bool ProjectModel::undo()
{
    endEdit();
    if (undoStack.empty())
        return false;
    redoStack.push_back (makeSnapshot());
    auto snapshot = std::move (undoStack.back());
    undoStack.pop_back();
    restoreSnapshot (std::move (snapshot));
    return true;
}

bool ProjectModel::redo()
{
    endEdit();
    if (redoStack.empty())
        return false;
    undoStack.push_back (makeSnapshot());
    auto snapshot = std::move (redoStack.back());
    redoStack.pop_back();
    restoreSnapshot (std::move (snapshot));
    return true;
}

bool ProjectModel::retainsAudioFile (const std::string& path) const
{
    const auto containsPath = [&path] (const std::vector<roll::Track>& snapshotTracks)
    {
        for (const auto& track : snapshotTracks)
            for (const auto& clip : track.audioClips)
                if (clip->filePath == path)
                    return true;
        return false;
    };

    for (const auto& track : tracks)
        for (const auto& clip : track->audioClips)
            if (clip->filePath == path)
                return true;
    for (const auto& snapshot : undoStack)
        if (containsPath (snapshot.tracks))
            return true;
    for (const auto& snapshot : redoStack)
        if (containsPath (snapshot.tracks))
            return true;
    return false;
}

roll::Track* ProjectModel::track (int id) const
{
    if (id == 0) return masterTrack.get();
    for (const auto& t : tracks) if (t->id == id) return t.get();
    return nullptr;
}

int ProjectModel::trackIndex (int id) const
{
    if (id == 0) return -1;
    for (size_t i = 0; i < tracks.size(); ++i) if (tracks[i]->id == id) return static_cast<int> (i);
    return -1;
}

roll::Placement* ProjectModel::selectedClip() const
{
    auto* t = selectedTrack();
    return t != nullptr ? t->find (selectedClipId) : nullptr;
}

roll::Track* ProjectModel::addTrack (roll::TrackType type)
{
    auto t = std::make_unique<roll::Track>();
    t->id = nextTrackId++;
    t->type = type;
    t->name = std::to_string (t->id) + (type == roll::TrackType::Audio ? " AUDIO" : " MIDI");
    auto* raw = t.get();
    tracks.push_back (std::move (t));
    if (onTrackAdded) onTrackAdded (raw->id);
    selectTrack (raw->id);
    sendChangeMessage();
    return raw;
}

void ProjectModel::removeTrack (int id)
{
    if (tracks.size() <= 1) return;                       // sempre resta uma trilha
    const int idx = trackIndex (id);
    if (idx < 0) return;

    if (selectedTrackId == id)
    {
        selectedClipId = -1;
        selectedAudioClipId = -1;
        if (onSelectionChanged) onSelectionChanged();     // o piano roll solta o clip antes dele ser apagado
    }
    if (onTrackRemoved) onTrackRemoved (id);
    tracks.erase (tracks.begin() + idx);

    if (selectedTrackId == id)
        selectedTrackId = tracks[static_cast<size_t> (std::min (idx, static_cast<int> (tracks.size()) - 1))]->id;
    if (onSelectionChanged) onSelectionChanged();
    if (onMixerChanged) onMixerChanged();
    sendChangeMessage();
}

void ProjectModel::selectTrack (int id)
{
    if (track (id) == nullptr) return;
    if (selectedTrackId != id)
    {
        selectedClipId = -1;
        selectedAudioClipId = -1;
    }
    selectedTrackId = id;
    if (onSelectionChanged) onSelectionChanged();
    sendChangeMessage();
}

void ProjectModel::selectClip (int trackId, int clipId)
{
    if (track (trackId) == nullptr) return;
    selectedTrackId = trackId;
    selectedClipId = clipId;
    selectedAudioClipId = -1;
    if (onSelectionChanged) onSelectionChanged();
    sendChangeMessage();
}

void ProjectModel::selectAudioClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    if (t == nullptr || t->findAudioClip (clipId) == nullptr) return;
    selectedTrackId = trackId;
    selectedClipId = -1;
    selectedAudioClipId = clipId;
    if (onSelectionChanged) onSelectionChanged();
    sendChangeMessage();
}

void ProjectModel::setTrackRouting (int trackId, roll::TrackOutputRouting routing)
{
    if (auto* t = track (trackId))
    {
        if (t->outputRouting != routing)
        {
            t->outputRouting = routing;
            notifyMixer();
        }
    }
}

roll::Placement* ProjectModel::addClip (int trackId, double start, double length)
{
    auto* t = track (trackId);
    if (t == nullptr || t->type != roll::TrackType::Midi || ! t->isFree (start, length)) return nullptr;
    recordUndo();
    auto* p = t->addClip (start, length);
    growTo (p->end() + 64.0);
    selectClip (trackId, p->id);
    notifyTrackNotes (trackId);
    return p;
}

int ProjectModel::copyClip (int fromTrack, int clipId, int toTrack, double start)
{
    auto* sourceTrack = track (fromTrack);
    auto* destinationTrack = track (toTrack);
    auto* source = sourceTrack != nullptr ? sourceTrack->find (clipId) : nullptr;
    if (source == nullptr || destinationTrack == nullptr)
        return -1;

    if (! destinationTrack->isFree (start, source->clip.length()))
        return -1;
    recordUndo();
    auto* copy = destinationTrack->copyClip (*source, start);
    if (copy == nullptr)
        return -1;

    growTo (copy->end() + 64.0);
    selectClip (toTrack, copy->id);
    notifyTrackNotes (toTrack);
    sendChangeMessage();
    return copy->id;
}

int ProjectModel::duplicateClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    auto* source = t != nullptr ? t->find (clipId) : nullptr;
    if (source == nullptr)
        return -1;

    const double length = source->clip.length();
    double start = source->end();
    for (int attempt = 0; attempt < 64; ++attempt, start += length)
        if (t->isFree (start, length))
        {
            recordUndo();
            auto* copy = t->copyClip (*source, start);
            if (copy == nullptr)
                return -1;
            growTo (copy->end() + 64.0);
            selectClip (trackId, copy->id);
            notifyTrackNotes (trackId);
            sendChangeMessage();
            return copy->id;
        }
    return -1;
}

roll::AudioPlacement* ProjectModel::addAudioClip (int trackId, double start, double length,
                                                   const std::string& filePath, const std::string& name,
                                                   const std::vector<float>& waveform,
                                                   double durationSeconds, double sourceBpm)
{
    auto* t = track (trackId);
    if (t == nullptr || t->type != roll::TrackType::Audio || start < 0.0 || length <= 0.0
        || filePath.empty() || durationSeconds < 0.0 || sourceBpm < 20.0 || sourceBpm > 300.0
        || ! t->isAudioFree (start, length))
        return nullptr;

    recordUndo();
    auto* clip = t->addAudioClip (start, length, filePath, name, waveform, durationSeconds, sourceBpm);
    growTo (clip->end() + 64.0);
    selectAudioClip (trackId, clip->id);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    return clip;
}

void ProjectModel::deleteClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    if (t == nullptr || t->find (clipId) == nullptr) return;

    recordUndo();
    if (selectedTrackId == trackId && selectedClipId == clipId)
    {
        selectedClipId = -1;
        if (onSelectionChanged) onSelectionChanged();
    }
    t->remove (clipId);
    notifyTrackNotes (trackId);
    sendChangeMessage();
}

int ProjectModel::splitClip (int trackId, int clipId, double beat)
{
    auto* t = track (trackId);
    auto* source = t != nullptr ? t->find (clipId) : nullptr;
    if (t == nullptr || source == nullptr || beat <= source->start
        || beat >= source->end() || ! t->isFree (beat, source->end() - beat, clipId))
        return -1;

    recordUndo();
    const int newId = t->splitClip (clipId, beat);
    if (newId < 0)
        return -1;

    growTo (beat + 64.0);
    selectClip (trackId, newId);
    notifyTrackNotes (trackId);
    sendChangeMessage();
    return newId;
}

void ProjectModel::deleteAudioClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    if (t == nullptr || t->findAudioClip (clipId) == nullptr) return;
    recordUndo();
    if (selectedTrackId == trackId && selectedAudioClipId == clipId)
    {
        selectedAudioClipId = -1;
        if (onSelectionChanged) onSelectionChanged();
    }
    t->removeAudioClip (clipId);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
}

int ProjectModel::copyAudioClip (int fromTrack, int clipId, int toTrack, double start)
{
    auto* sourceTrack = track (fromTrack);
    auto* destinationTrack = track (toTrack);
    auto* source = sourceTrack != nullptr ? sourceTrack->findAudioClip (clipId) : nullptr;
    if (source == nullptr || destinationTrack == nullptr)
        return -1;

    if (! destinationTrack->isAudioFree (start, source->lengthBeats))
        return -1;
    recordUndo();
    auto* copy = destinationTrack->copyAudioClip (*source, start);
    if (copy == nullptr)
        return -1;

    growTo (copy->end() + 64.0);
    selectAudioClip (toTrack, copy->id);
    if (onAudioClipsChanged) onAudioClipsChanged (toTrack);
    sendChangeMessage();
    return copy->id;
}

int ProjectModel::duplicateAudioClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    auto* source = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (source == nullptr)
        return -1;

    const double length = source->lengthBeats;
    double start = source->end();
    for (int attempt = 0; attempt < 64; ++attempt, start += length)
        if (t->isAudioFree (start, length))
        {
            recordUndo();
            auto* copy = t->copyAudioClip (*source, start);
            if (copy == nullptr)
                return -1;
            growTo (copy->end() + 64.0);
            selectAudioClip (trackId, copy->id);
            if (onAudioClipsChanged) onAudioClipsChanged (trackId);
            sendChangeMessage();
            return copy->id;
        }
    return -1;
}

int ProjectModel::splitAudioClip (int trackId, int clipId, double beat)
{
    auto* t = track (trackId);
    auto* source = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (t == nullptr || source == nullptr || beat <= source->start
        || beat >= source->end() || ! t->isAudioFree (beat, source->end() - beat, clipId))
        return -1;

    recordUndo();
    const int newId = t->splitAudioClip (clipId, beat);
    if (newId < 0)
        return -1;

    if (auto* clip = t->findAudioClip (newId))
        growTo (clip->end() + 64.0);
    selectAudioClip (trackId, newId);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
    return newId;
}

bool ProjectModel::resizeAudioClip (int trackId, int clipId, double length)
{
    auto* t = track (trackId);
    auto* clip = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (t == nullptr || t->type != roll::TrackType::Audio || clip == nullptr || length <= 0.0
        || ! t->isAudioFree (clip->start, length, clipId))
        return false;
    recordUndo();
    clip->lengthBeats = length;
    clip->fadeInBeats = std::min (clip->fadeInBeats, length);
    clip->fadeOutBeats = std::min (clip->fadeOutBeats, length);
    growTo (clip->end() + 64.0);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
    return true;
}

bool ProjectModel::setAudioClipFades (int trackId, int clipId, double fadeInBeats,
                                      double fadeOutBeats)
{
    auto* t = track (trackId);
    auto* clip = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (t == nullptr || t->type != roll::TrackType::Audio || clip == nullptr
        || ! std::isfinite (fadeInBeats) || ! std::isfinite (fadeOutBeats)
        || fadeInBeats < 0.0 || fadeOutBeats < 0.0
        || fadeInBeats > clip->lengthBeats || fadeOutBeats > clip->lengthBeats)
        return false;

    if (clip->fadeInBeats == fadeInBeats && clip->fadeOutBeats == fadeOutBeats)
        return true;

    recordUndo();
    clip->fadeInBeats = fadeInBeats;
    clip->fadeOutBeats = fadeOutBeats;
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
    return true;
}

bool ProjectModel::setAudioClipSourceBpm (int trackId, int clipId, double sourceBpm)
{
    auto* t = track (trackId);
    auto* clip = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (t == nullptr || t->type != roll::TrackType::Audio || clip == nullptr
        || sourceBpm < 20.0 || sourceBpm > 300.0 || clip->durationSeconds <= 0.0)
        return false;

    const double newLength = clip->durationSeconds * sourceBpm / 60.0;
    if (! t->isAudioFree (clip->start, newLength, clipId))
        return false;

    recordUndo();
    clip->sourceBpm = sourceBpm;
    clip->lengthBeats = newLength;
    clip->fadeInBeats = std::min (clip->fadeInBeats, newLength);
    clip->fadeOutBeats = std::min (clip->fadeOutBeats, newLength);
    growTo (clip->end() + 64.0);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
    return true;
}

int ProjectModel::moveClip (int fromTrack, int clipId, int toTrack, double newStart)
{
    auto* src = track (fromTrack);
    auto* dst = track (toTrack);
    if (src == nullptr || dst == nullptr
        || src->type != roll::TrackType::Midi || dst->type != roll::TrackType::Midi) return -1;
    auto* p = src->find (clipId);
    if (p == nullptr) return -1;

    const double len = p->clip.length();
    newStart = std::max (0.0, newStart);

    if (fromTrack == toTrack)
    {
        if (! src->isFree (newStart, len, clipId)) return -1;
        if (std::abs (p->start - newStart) <= 1.0e-9) return clipId;
        recordUndo();
        p->start = newStart;
        growTo (p->end() + 64.0);
        notifyTrackNotes (fromTrack);
        sendChangeMessage();
        return clipId;
    }

    if (! dst->isFree (newStart, len)) return -1;
    recordUndo();
    auto taken = src->take (clipId);
    taken->start = newStart;
    dst->put (std::move (taken));
    const int newId = dst->clips.back()->id;

    if (selectedTrackId == fromTrack && selectedClipId == clipId)
    {
        selectedTrackId = toTrack;
        selectedClipId = newId;
    }
    growTo (newStart + len + 64.0);
    notifyTrackNotes (fromTrack);
    notifyTrackNotes (toTrack);
    if (onSelectionChanged) onSelectionChanged();
    sendChangeMessage();
    return newId;
}

int ProjectModel::moveAudioClip (int fromTrack, int clipId, int toTrack, double newStart)
{
    auto* src = track (fromTrack);
    auto* dst = track (toTrack);
    if (src == nullptr || dst == nullptr
        || src->type != roll::TrackType::Audio || dst->type != roll::TrackType::Audio)
        return -1;
    auto* clip = src->findAudioClip (clipId);
    if (clip == nullptr) return -1;
    newStart = std::max (0.0, newStart);
    const double length = clip->lengthBeats;

    if (fromTrack == toTrack)
    {
        if (! src->isAudioFree (newStart, length, clipId)) return -1;
        if (std::abs (clip->start - newStart) <= 1.0e-9) return clipId;
        recordUndo();
        clip->start = newStart;
        growTo (clip->end() + 64.0);
        if (onAudioClipsChanged) onAudioClipsChanged (fromTrack);
        sendChangeMessage();
        return clipId;
    }

    if (! dst->isAudioFree (newStart, length)) return -1;
    recordUndo();
    auto moved = src->takeAudioClip (clipId);
    moved->start = newStart;
    dst->putAudioClip (std::move (moved));
    const int newId = dst->audioClips.back()->id;
    if (selectedTrackId == fromTrack && selectedAudioClipId == clipId)
        selectAudioClip (toTrack, newId);
    growTo (newStart + length + 64.0);
    if (onAudioClipsChanged) onAudioClipsChanged (fromTrack);
    if (onAudioClipsChanged) onAudioClipsChanged (toTrack);
    sendChangeMessage();
    return newId;
}

void ProjectModel::notifyTrackNotes (int trackId)
{
    if (onTrackNotes) onTrackNotes (trackId);
}

void ProjectModel::setLoop (double s, double e)
{
    loopStart = std::max (0.0, s);
    loopEnd = std::max (loopStart + 1.0 / 64.0, e);
    if (onLoopChanged) onLoopChanged();
    sendChangeMessage();
}

void ProjectModel::growTo (double beats)
{
    if (beats > totalBeats) totalBeats = std::ceil (beats / 16.0) * 16.0;
}
