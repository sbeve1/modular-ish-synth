cmake_minimum_required(VERSION 3.15)
project(Synth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# Platform definitions
# --------------------------------------------------
add_compile_definitions(__WINDOWS_MM__)

# --------------------------------------------------
# Collect source files
# --------------------------------------------------
file(GLOB ROOT_SRC
    "src/*.cpp"
)

file(GLOB CORE_SRC
    "src/core/*.cpp"
)

file(GLOB ENGINE_SRC
    "src/engine/*.cpp"
)

file(GLOB ENGINE_VOICE_SRC
    "src/engine/voice/*.cpp"
)

file(GLOB AUDIO_SRC
    "src/audio/*.cpp"
)

file(GLOB MIDI_SRC
    "src/midi/*.cpp"
)

file(GLOB PLATFORM_SRC
    "src/platform/*.cpp"
    "external/*.cpp"
)

file(GLOB_RECURSE AUDIO_SRC
    "src/audio/*.cpp"
)


# IMPORTANT: control sources (LFO.cpp lives here)
file(GLOB CONTROL_SRC
    "src/control/*.cpp"
)


# --------------------------------------------------
# Build executable
# --------------------------------------------------
add_executable(Synth
    ${ROOT_SRC}
    ${CORE_SRC}
    ${ENGINE_SRC}
    ${ENGINE_VOICE_SRC}
    ${AUDIO_SRC}
    ${MIDI_SRC}
    ${PLATFORM_SRC}
    ${CONTROL_SRC}
)

# --------------------------------------------------
# Include paths
# --------------------------------------------------
target_include_directories(Synth PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

# --------------------------------------------------
# Windows system libraries
# --------------------------------------------------
if (WIN32)
    target_link_libraries(Synth winmm)
endif()
