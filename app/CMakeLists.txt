# Pages and helpers that exist only in the standalone application. Kept in an INTERFACE library so the
# JUCE-level tests can compile exactly the same sources.
add_library(realtone_app_shared INTERFACE)
target_sources(realtone_app_shared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/MainView.cpp
    ${CMAKE_SOURCE_DIR}/studio/AppContext.cpp
    ${CMAKE_SOURCE_DIR}/studio/PreviewPlayer.cpp
    ${CMAKE_SOURCE_DIR}/studio/OfflineWorkflows.cpp
    ${CMAKE_SOURCE_DIR}/studio/WindowGeometry.cpp
    ${CMAKE_SOURCE_DIR}/ui/WaveformView.cpp
    ${CMAKE_SOURCE_DIR}/ui/NoteRoll.cpp
    ${CMAKE_SOURCE_DIR}/ui/ExportOptions.cpp
    ${CMAKE_SOURCE_DIR}/ui/JobBar.cpp
    ${CMAKE_SOURCE_DIR}/ui/MidiRenderPanel.cpp
    ${CMAKE_SOURCE_DIR}/ui/AudioConvertPanel.cpp
    ${CMAKE_SOURCE_DIR}/ui/SettingsPanel.cpp)
target_link_libraries(realtone_app_shared INTERFACE realtone_studio)

juce_add_gui_app(RealToneStudio
    PRODUCT_NAME "RealTone Studio"
    COMPANY_NAME "RealTone"
    DOCUMENT_EXTENSIONS rtproject
    VERSION ${PROJECT_VERSION})

target_sources(RealToneStudio PRIVATE Main.cpp)
target_compile_definitions(RealToneStudio PRIVATE
    JUCE_APPLICATION_NAME_STRING="RealTone Studio"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(RealToneStudio PRIVATE realtone_app_shared)
