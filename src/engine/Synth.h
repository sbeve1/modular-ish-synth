#pragma once

#include <array>

#include "engine/Voice.h"
#include "engine/Bus.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "audio/WavetableFactory.h"

class Synth {
public:
    static constexpr int NUM_VOICES = 16;

    std::array<Voice, NUM_VOICES> voices;

    // FIX: unique note instance counter for voice stealing / release
    int nextNoteId = 0;

    // ---- GLOBAL PARAM BLOCKS (KEEP THESE) ----
    OscParams oscAParams;
    OscParams oscBParams;
    OscParams oscCParams;
    OscParams subParams;
    NoiseParams noiseParams;

    // ---- BUSES (KEEP THESE) ----
    Bus bus1;
    Bus bus2;

    Wavetable basicWavetable;

    Synth();

    void noteOn(int note, float velocity);
    void noteOff(int note);
    Sample process(float sampleRate);
};
