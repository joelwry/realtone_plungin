# Code shared by the standalone application and the VST3 plug-in: the processor, the instrument
# manager, presets and every UI component. It is an INTERFACE library so that each final target
# compiles these sources together with its own copy of the JUCE modules (the pattern JUCE
# recommends for shared code).

juce_add_binary_data(RealToneResources
    SOURCES
        ${CMAKE_SOURCE_DIR}/libraries/manifests/vsco2ce.json
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.txt)

set(REALTONE_STUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Paths.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JuceFetcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/InstrumentManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Resources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RealToneProcessor.cpp
    ${CMAKE_SOURCE_DIR}/ui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/ui/InstrumentArt.cpp
    ${CMAKE_SOURCE_DIR}/ui/InstrumentBrowser.cpp
    ${CMAKE_SOURCE_DIR}/ui/PitchBendWheel.cpp
    ${CMAKE_SOURCE_DIR}/ui/PerformancePanel.cpp
    ${CMAKE_SOURCE_DIR}/ui/PresetBar.cpp
    ${CMAKE_SOURCE_DIR}/ui/StatusStrip.cpp
    ${CMAKE_SOURCE_DIR}/ui/StudioView.cpp
    ${CMAKE_SOURCE_DIR}/ui/TabShell.cpp
    ${CMAKE_SOURCE_DIR}/ui/LibraryPanel.cpp
    ${CMAKE_SOURCE_DIR}/ui/AboutPanel.cpp
    ${CMAKE_SOURCE_DIR}/ui/PluginEditor.cpp
)
set(REALTONE_STANDALONE_SOURCES "" CACHE INTERNAL "")

add_library(realtone_studio INTERFACE)
target_sources(realtone_studio INTERFACE ${REALTONE_STUDIO_SOURCES})
target_include_directories(realtone_studio INTERFACE ${CMAKE_SOURCE_DIR})
target_compile_definitions(realtone_studio INTERFACE
    REALTONE_VERSION="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(realtone_studio INTERFACE
    realtone_core
    RealToneResources
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)
