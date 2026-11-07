#include <catch2/catch_test_macros.hpp>
#include "Clip.h"
#include "History.h"

using namespace roll;

TEST_CASE("snapToGrid arredonda para a grade")
{
    REQUIRE(snapToGrid(0.12, 0.25) == 0.0);
    REQUIRE(snapToGrid(0.13, 0.25) == 0.25);
    REQUIRE(snapToGrid(1.9, 0.5)   == 2.0);
}

TEST_CASE("Clip adiciona, encontra e remove notas por id")
{
    Clip clip(8.0);
    int a = clip.addNote({ 60, 1.0, 2.0, 100 });
    int b = clip.addNote({ 64, 0.0, 1.0, 100 });
    REQUIRE(a != b);
    REQUIRE(clip.noteAt(60, 1.5) == a);
    REQUIRE(clip.noteAt(60, 3.0) == -1);   // fim exclusivo
    REQUIRE(clip.noteAt(61, 1.5) == -1);
    REQUIRE(clip.removeNote(a));
    REQUIRE_FALSE(clip.removeNote(a));
    REQUIRE(clip.find(b) != nullptr);
    REQUIRE(clip.notes().size() == 1);
}

TEST_CASE("setNotes preserva ids e nao reutiliza")
{
    Clip clip(8.0);
    int a = clip.addNote({ 60, 0.0, 1.0, 100 });
    auto saved = clip.notes();
    clip.removeNote(a);
    clip.setNotes(saved);
    int c = clip.addNote({ 62, 0.0, 1.0, 100 });
    REQUIRE(c != a);
}

TEST_CASE("History desfaz e refaz")
{
    Clip clip(8.0);
    History h;
    h.record(clip.notes());                 // estado vazio
    clip.addNote({ 60, 0.0, 1.0, 100 });
    REQUIRE(h.canUndo());

    auto cur = clip.notes();
    REQUIRE(h.undo(cur));
    REQUIRE(cur.empty());
    REQUIRE(h.canRedo());
    REQUIRE(h.redo(cur));
    REQUIRE(cur.size() == 1);
    REQUIRE_FALSE(h.canRedo());
}

TEST_CASE("Nota nova apaga a que ela cobre pelo inicio")
{
    Clip clip(8.0);
    int old = clip.addNote({ 60, 2.0, 1.0, 100 });
    int fresh = clip.addNote({ 60, 1.5, 2.0, 100 });        // cobre o inicio da antiga
    clip.resolveOverlaps({ fresh });
    REQUIRE(clip.find(old) == nullptr);
    REQUIRE(clip.find(fresh) != nullptr);
}

TEST_CASE("Nota nova encurta a que ela cobre pela cauda")
{
    Clip clip(8.0);
    int old = clip.addNote({ 60, 0.0, 3.0, 100 });
    int fresh = clip.addNote({ 60, 2.0, 1.0, 100 });
    clip.resolveOverlaps({ fresh });
    REQUIRE(clip.find(old) != nullptr);
    REQUIRE(clip.find(old)->lengthBeats == 2.0);
}

TEST_CASE("Pitches diferentes nao interferem")
{
    Clip clip(8.0);
    int a = clip.addNote({ 60, 0.0, 3.0, 100 });
    int b = clip.addNote({ 62, 1.0, 1.0, 100 });
    clip.resolveOverlaps({ b });
    REQUIRE(clip.find(a)->lengthBeats == 3.0);
}
