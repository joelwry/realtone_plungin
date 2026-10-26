cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime: the installer and the portable zip must not depend on the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")
endif()

project(RealToneStudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(REALTONE_BUILD_TESTS "Build the unit and integration tests" ON)
option(REALTONE_BUILD_APP "Build the standalone application and the VST3 plugin (needs JUCE)" ON)
set(REALTONE_JUCE_DIR "" CACHE PATH "Use an existing JUCE checkout instead of fetching one")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8 /bigobj /MP)
endif()

include(CTest)

add_subdirectory(core)
add_subdirectory(cli)

if(REALTONE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(REALTONE_BUILD_APP)
    include(cmake/Juce.cmake)
    add_subdirectory(studio)
    add_subdirectory(plugin)
    add_subdirectory(app)
    if(REALTONE_BUILD_TESTS)
        add_subdirectory(tests_studio)
    endif()
endif()
