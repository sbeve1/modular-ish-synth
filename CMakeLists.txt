cmake_minimum_required(VERSION 3.15)
project(Synth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------
# Force RtMidi to use the WinMM backend
# ---------------------------------------
add_compile_definitions(__WINDOWS_MM__)

# ---------------------------------------
# Collect source files
# ---------------------------------------

file(GLOB ROOT_SRC "src/*.cpp")
file(GLOB CORE_SRC "src/core/*.cpp")
file(GLOB ENGINE_SRC "src/engine/*.cpp")
file(GLOB AUDIO_SRC "src/audio/*.cpp")
file(GLOB MIDI_SRC "src/midi/*.cpp")
file(GLOB PLATFORM_SRC "src/platform/*.cpp" "external/*.cpp")

# ---------------------------------------
# Build executable
# ---------------------------------------

add_executable(Synth
    ${ROOT_SRC}
    ${CORE_SRC}
    ${ENGINE_SRC}
    ${AUDIO_SRC}
    ${MIDI_SRC}
    ${PLATFORM_SRC}
)

# ---------------------------------------
# Includes
# ---------------------------------------

target_include_directories(Synth PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/audio
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/midi
    ${PROJECT_SOURCE_DIR}/src/platform
    ${PROJECT_SOURCE_DIR}/external
)

# ---------------------------------------
# Windows-specific settings
# ---------------------------------------

if (WIN32)
    target_link_libraries(Synth winmm)
endif()
