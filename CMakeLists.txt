cmake_minimum_required(VERSION 3.22)
project(ProjectFreedom VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# ---- Core: modelo musical puro, SEM JUCE (testavel e reutilizavel) ----
add_subdirectory(core)

# ---- Testes do core ----
option(ROLL_BUILD_TESTS "Build core unit tests" ON)
if(ROLL_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.1)
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---- App (JUCE) ----
option(ROLL_BUILD_APP "Build the JUCE application" ON)
if(ROLL_BUILD_APP)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(app)
endif()
