#include <catch2/catch_test_macros.hpp>
#include "Scale.h"

using namespace roll;

TEST_CASE("C major contém apenas teclas brancas")
{
    Scale cMajor(0, Mode::Major);
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) REQUIRE(cMajor.contains(60 + pc));
    for (int pc : { 1, 3, 6, 8, 10 })       REQUIRE_FALSE(cMajor.contains(60 + pc));
}

TEST_CASE("Funciona em qualquer oitava")
{
    Scale s(0, Mode::Major);
    REQUIRE(s.contains(0));
    REQUIRE(s.contains(127));        // G
    REQUIRE_FALSE(s.contains(126));  // F#
}

TEST_CASE("A menor natural tem as mesmas notas de C maior")
{
    Scale aMinor(9, Mode::NaturalMinor);
    Scale cMajor(0, Mode::Major);
    for (int n = 0; n < 128; ++n)
        REQUIRE(aMinor.contains(n) == cMajor.contains(n));
}

TEST_CASE("Tônica fora de 0..11 é normalizada")
{
    REQUIRE(Scale(-3, Mode::Major).root() == 9);
    REQUIRE(Scale(14, Mode::Major).root() == 2);
}
