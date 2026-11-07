juce_add_gui_app(ProjectFreedom
    PRODUCT_NAME "Project Freedom"
    COMPANY_NAME "rjc"
    VERSION ${PROJECT_VERSION})

target_sources(ProjectFreedom PRIVATE
    Main.cpp
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
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_gui_basics
    PRIVATE
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
