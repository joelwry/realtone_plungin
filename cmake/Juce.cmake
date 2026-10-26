# JUCE is pulled in with FetchContent at a pinned release unless an existing checkout is given.
# JUCE 8 is dual licensed (AGPLv3 or commercial); see docs/DEPENDENCIES.md before distributing binaries.

set(REALTONE_JUCE_TAG "8.0.4" CACHE STRING "JUCE release tag to fetch")

if(REALTONE_JUCE_DIR)
    message(STATUS "Using JUCE from ${REALTONE_JUCE_DIR}")
    add_subdirectory(${REALTONE_JUCE_DIR} ${CMAKE_BINARY_DIR}/_juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${REALTONE_JUCE_TAG}
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(juce)
endif()

# Plugin SDK location is handled by JUCE (it bundles the VST3 SDK, MIT licensed since 3.8).
