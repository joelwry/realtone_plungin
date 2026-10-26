# JUCE-level tests: processor behaviour through processBlock, state, presets, instrument manager,
# editor layout at several sizes. Runs headless (use xvfb-run on Linux CI).
juce_add_console_app(realtone_studio_tests PRODUCT_NAME "realtone_studio_tests")
target_sources(realtone_studio_tests PRIVATE
    StudioMain.cpp
    ProcessorTests.cpp
    UiTests.cpp
    AppTests.cpp
)
target_include_directories(realtone_studio_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(realtone_studio_tests PRIVATE realtone_app_shared)
add_test(NAME realtone_studio_tests COMMAND realtone_studio_tests)
set_tests_properties(realtone_studio_tests PROPERTIES ENVIRONMENT "REALTONE_TEST_OUTPUT=${CMAKE_BINARY_DIR}/test-output")
target_compile_definitions(realtone_studio_tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1
    REALTONE_TEST_DATA="${CMAKE_SOURCE_DIR}/tests/data")
