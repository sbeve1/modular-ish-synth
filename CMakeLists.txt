cmake_minimum_required(VERSION 3.15)
project(Synth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# Platform definitions
# --------------------------------------------------
add_compile_definitions(__WINDOWS_MM__)

# MSVC: keep windows.h from defining min/max macros, and expose M_PI
add_compile_definitions(NOMINMAX _USE_MATH_DEFINES)

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


file(GLOB INPUT_SRC
    "src/input/*.cpp"
)

file(GLOB RENDER_SRC
    "src/render/*.cpp"
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
    ${RENDER_SRC}
    ${INPUT_SRC}
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

# --------------------------------------------------
# GUI instrument (app/)
# --------------------------------------------------
option(SYNTH_BUILD_APP "Build the GUI synthesizer app" ON)
if (SYNTH_BUILD_APP)
    add_subdirectory(app)
endif()
