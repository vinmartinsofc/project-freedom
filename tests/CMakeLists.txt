add_executable(roll_tests
    ScaleTests.cpp
    ClipTests.cpp
    SequencerTests.cpp
    ArrangementTests.cpp
    PluginFolderGroupingTests.cpp
    WindowPlacementTests.cpp)
target_link_libraries(roll_tests PRIVATE roll_core Catch2::Catch2WithMain)
include(CTest)
include(Catch)
catch_discover_tests(roll_tests)
