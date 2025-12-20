#pragma once

#include <array>

#include "engine/Voice.h"
#include "engine/Bus.h"
#include "engine/MasterLimiter.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "audio/WavetableFactory.h"

class Synth {
public:
    static constexpr int NUM_VOICES = 16;

    std::array<Voice, NUM_VOICES> voices;

    OscParams oscAParams;
    OscParams oscBParams;
    OscParams oscCParams;
    OscParams subParams;
    NoiseParams noiseParams;

    Bus bus1;
    Bus bus2;

    MasterLimiter limiter;

    int nextVoice = 0;

    Wavetable basicWavetable;

    Synth();

    void noteOn(int note, float velocity);
    void noteOff(int note);
    Sample process(float sampleRate);
};
