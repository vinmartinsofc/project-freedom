juce_add_gui_app(ProjectFreedom
    PRODUCT_NAME "Project Freedom"
    COMPANY_NAME "rjc"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app/project-freedom.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/app/project-freedom.png"
    VERSION ${PROJECT_VERSION})

file(GLOB_RECURSE PROJECT_FREEDOM_ICON_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
juce_add_binary_data(ProjectFreedomIcons SOURCES ${PROJECT_FREEDOM_ICON_FILES})

target_sources(ProjectFreedom PRIVATE
    Main.cpp
    Icons.cpp
    ProjectModel.cpp
    ArrangementView.cpp
    DeviceView.cpp
    AudioEngine.cpp
    PluginScanner.cpp
    BrowserPanel.cpp
    TransportBar.cpp
    RollModel.cpp
    PianoRollEditor.cpp
    MainComponent.cpp)

target_compile_definitions(ProjectFreedom PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ProjectFreedom,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ProjectFreedom,JUCE_VERSION>")

target_link_libraries(ProjectFreedom
    PRIVATE
        roll_core
        ProjectFreedomIcons
        juce::juce_data_structures
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_gui_basics
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
