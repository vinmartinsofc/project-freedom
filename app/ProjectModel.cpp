#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

roll::Track* ProjectModel::track (int id) const
{
    for (const auto& t : tracks) if (t->id == id) return t.get();
    return nullptr;
}

int ProjectModel::trackIndex (int id) const
{
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

roll::Placement* ProjectModel::addClip (int trackId, double start, double length)
{
    auto* t = track (trackId);
    if (t == nullptr || t->type != roll::TrackType::Midi || ! t->isFree (start, length)) return nullptr;
    auto* p = t->addClip (start, length);
    growTo (p->end() + 64.0);
    selectClip (trackId, p->id);
    notifyTrackNotes (trackId);
    return p;
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

    if (selectedTrackId == trackId && selectedClipId == clipId)
    {
        selectedClipId = -1;
        if (onSelectionChanged) onSelectionChanged();
    }
    t->remove (clipId);
    notifyTrackNotes (trackId);
    sendChangeMessage();
}

void ProjectModel::deleteAudioClip (int trackId, int clipId)
{
    auto* t = track (trackId);
    if (t == nullptr || t->findAudioClip (clipId) == nullptr) return;
    if (selectedTrackId == trackId && selectedAudioClipId == clipId)
    {
        selectedAudioClipId = -1;
        if (onSelectionChanged) onSelectionChanged();
    }
    t->removeAudioClip (clipId);
    if (onAudioClipsChanged) onAudioClipsChanged (trackId);
    sendChangeMessage();
}

bool ProjectModel::resizeAudioClip (int trackId, int clipId, double length)
{
    auto* t = track (trackId);
    auto* clip = t != nullptr ? t->findAudioClip (clipId) : nullptr;
    if (t == nullptr || t->type != roll::TrackType::Audio || clip == nullptr || length <= 0.0
        || ! t->isAudioFree (clip->start, length, clipId))
        return false;
    clip->lengthBeats = length;
    growTo (clip->end() + 64.0);
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

    clip->sourceBpm = sourceBpm;
    clip->lengthBeats = newLength;
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
        p->start = newStart;
        growTo (p->end() + 64.0);
        notifyTrackNotes (fromTrack);
        sendChangeMessage();
        return clipId;
    }

    if (! dst->isFree (newStart, len)) return -1;
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
        clip->start = newStart;
        growTo (clip->end() + 64.0);
        if (onAudioClipsChanged) onAudioClipsChanged (fromTrack);
        sendChangeMessage();
        return clipId;
    }

    if (! dst->isAudioFree (newStart, length)) return -1;
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
    loopEnd = std::max (loopStart + 1.0, e);
    if (onLoopChanged) onLoopChanged();
    sendChangeMessage();
}

void ProjectModel::growTo (double beats)
{
    if (beats > totalBeats) totalBeats = std::ceil (beats / 16.0) * 16.0;
}
