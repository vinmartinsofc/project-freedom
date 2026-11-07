#pragma once
#include "Note.h"
#include <vector>

namespace roll
{
// Arredonda 'beats' para a grade mais próxima (gridBeats = 0.25 -> 1/16).
double snapToGrid(double beats, double gridBeats);

class Clip
{
public:
    explicit Clip(double lengthInBeats = 16.0) : lengthBeats(lengthInBeats) {}

    double length() const { return lengthBeats; }
    void   setLength(double beats) { lengthBeats = beats; }

    int  addNote(Note n);                       // atribui e retorna o id
    bool removeNote(int id);
    Note*       find(int id);
    const Note* find(int id) const;
    int  noteAt(int pitch, double beat) const;  // id, ou -1 se não houver

    const std::vector<Note>& notes() const { return notesVec; }
    std::vector<Note>&       notes()       { return notesVec; }

    // Regra do Live: nota nova sobre outra do mesmo pitch -> a coberta pelo inicio some,
    // a coberta pela cauda é encurtada ate o inicio da nova. 'ids' = notas recem movidas/criadas.
    void resolveOverlaps (const std::vector<int>& ids);

    // Substitui todas as notas (desfazer/refazer). Ids são preservados.
    void setNotes(std::vector<Note> newNotes);

private:
    double lengthBeats;
    int nextId = 1;
    std::vector<Note> notesVec;
};
} // namespace roll
