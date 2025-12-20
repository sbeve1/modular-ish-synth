#pragma once

#include <array>

#include "engine/Voice.h"
#include "engine/SourceRouter.h"
#include "engine/SourceParams.h"
#include "engine/Bus.h"
#include "engine/VoiceAmp.h"
#include "engine/MasterLimiter.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "audio/WavetableFactory.h"

class Synth {
public:
    static constexpr int NUM_VOICES = 16;

    std::array<Voice, NUM_VOICES> voices;

    SourceParams oscAParams;
    SourceParams oscBParams;
    SourceParams oscCParams;
    SourceParams subParams;
    SourceParams noiseParams;

    SourceRouter router;

    Bus bus1;
    Bus bus2;

    VoiceAmp      amp;
    MasterLimiter limiter;

    int nextVoice = 0;

    Wavetable basicWavetable;

    Synth();

    void noteOn(int note, float velocity);
    void noteOff(int note);
    Sample process(float sampleRate);
};
