#pragma once

#include <array>

#include "engine/voice/Voice.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "audio/WavetableFactory.h"

class Synth {
public:
    static constexpr int NUM_VOICES = 8;

    std::array<Voice, NUM_VOICES> voices;

    // FIX: unique note instance counter for voice stealing / release
    int nextNoteId = 0;

    // ---- GLOBAL PARAM BLOCKS (KEEP THESE) ----
    OscParams oscAParams;
    OscParams oscBParams;
    OscParams oscCParams;
    OscParams subParams;
    NoiseParams noiseParams;

    Wavetable basicWavetable;

    Synth();

    void noteOn(int note, float velocity);
    void noteOff(int note);
    Sample process(float sampleRate);
};
