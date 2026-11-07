add_library(roll_core STATIC
    Scale.cpp
    Clip.cpp
    History.cpp
    Sequencer.cpp
    Arrangement.cpp)
target_include_directories(roll_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(roll_core PUBLIC cxx_std_20)
