#include "History.h"

namespace roll
{
void History::record(const std::vector<Note>& before)
{
    undoStack.push_back(before);
    if (undoStack.size() > maxDepth)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

bool History::undo(std::vector<Note>& current)
{
    if (undoStack.empty()) return false;
    redoStack.push_back(current);
    current = std::move(undoStack.back());
    undoStack.pop_back();
    return true;
}

bool History::redo(std::vector<Note>& current)
{
    if (redoStack.empty()) return false;
    undoStack.push_back(current);
    current = std::move(redoStack.back());
    redoStack.pop_back();
    return true;
}
} // namespace roll
