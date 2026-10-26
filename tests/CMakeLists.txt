add_executable(realtone_tests
    main.cpp
    AllocCounter.cpp
    EngineTests.cpp
    IoTests.cpp
    MidiRenderTests.cpp
    AnalysisTests.cpp
    RealtimeTests.cpp
)
target_link_libraries(realtone_tests PRIVATE realtone_core)
target_include_directories(realtone_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT MSVC)
    target_compile_options(realtone_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_test(NAME realtone_tests COMMAND realtone_tests)
