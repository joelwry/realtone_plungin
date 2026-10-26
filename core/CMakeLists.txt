add_library(realtone_core STATIC
    src/AudioClip.cpp
    src/AudioFile.cpp
    src/Sha256.cpp
    src/Resampler.cpp
    src/Sfz.cpp
    src/Instrument.cpp
    src/InstrumentProfile.cpp
    src/SamplerEngine.cpp
    src/InstrumentSwap.cpp
    src/MidiFile.cpp
    src/Renderer.cpp
    src/PitchAnalysis.cpp
    src/Library.cpp
)

target_include_directories(realtone_core
    PUBLIC include
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/dr_libs
)
target_include_directories(realtone_core SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party)
target_compile_features(realtone_core PUBLIC cxx_std_17)

find_package(Threads REQUIRED)
target_link_libraries(realtone_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(realtone_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(realtone_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
