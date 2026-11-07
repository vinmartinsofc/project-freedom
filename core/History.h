#pragma once
#include "Note.h"
#include <vector>

namespace roll
{
// Desfazer/refazer por snapshots das notas do clip. Simples e robusto:
// clips tem poucas centenas de notas, então copiar a lista é barato.
class History
{
public:
    // Chame ANTES de modificar. Guarda o estado atual e invalida o "refazer".
    void record(const std::vector<Note>& before);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    // Troca 'current' pelo estado anterior/seguinte. Retorna false se não há o que fazer.
    bool undo(std::vector<Note>& current);
    bool redo(std::vector<Note>& current);

    void clear() { undoStack.clear(); redoStack.clear(); }

private:
    static constexpr size_t maxDepth = 200;
    std::vector<std::vector<Note>> undoStack, redoStack;
};
} // namespace roll
