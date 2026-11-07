#include <catch2/catch_test_macros.hpp>
#include "Arrangement.h"

using namespace roll;

TEST_CASE("Clips nao podem se sobrepor na mesma trilha")
{
    Track t;
    auto* a = t.addClip(0.0, 4.0);
    REQUIRE(t.isFree(4.0, 4.0));              // encostar e permitido
    REQUIRE_FALSE(t.isFree(3.0, 2.0));
    REQUIRE(t.isFree(0.0, 4.0, a->id));       // ignora a si mesmo
}

TEST_CASE("flatten converte notas do clip para tempo absoluto")
{
    Track t;
    auto* c = t.addClip(8.0, 4.0);             // comeca no beat 8
    c->clip.addNote({ 60, 1.0, 1.0, 100 });
    auto flat = flatten(t);
    REQUIRE(flat.size() == 1);
    REQUIRE(flat[0].startBeats == 9.0);
}

TEST_CASE("flatten corta notas que passam do fim do clip e descarta as de fora")
{
    Track t;
    auto* c = t.addClip(0.0, 4.0);
    c->clip.addNote({ 60, 3.5, 2.0, 100 });    // sai do clip: cortada em 0.5
    c->clip.addNote({ 62, 5.0, 1.0, 100 });    // totalmente fora
    auto flat = flatten(t);
    REQUIRE(flat.size() == 1);
    REQUIRE(flat[0].lengthBeats == 0.5);
}

TEST_CASE("Mover clip entre trilhas preserva as notas")
{
    Track a, b;
    auto* c = a.addClip(0.0, 4.0);
    c->clip.addNote({ 60, 0.0, 1.0, 100 });
    int id = c->id;
    auto taken = a.take(id);
    REQUIRE(a.clips.empty());
    b.put(std::move(taken));
    REQUIRE(b.clips.size() == 1);
    REQUIRE(b.clips[0]->clip.notes().size() == 1);
}
