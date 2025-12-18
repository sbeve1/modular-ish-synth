cmake_minimum_required(VERSION 3.15)
project(Synth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/core/main.cpp
    src/core/Synth.cpp
    src/midi/MIDIHandler.cpp
    external/RtMidi.cpp
)

add_executable(Synth ${SOURCES})

target_include_directories(Synth PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external
)

# IMPORTANT: Enable Windows MIDI backend in RtMidi!
target_compile_definitions(Synth PRIVATE __WINDOWS_MM__)

# Link Windows Multimedia (required)
if (WIN32)
    target_link_libraries(Synth winmm)
endif()
