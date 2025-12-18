#pragma once
#include <array>
#include <vector>
#include "Voice.h"
#include "ModTargetResolver.h"

struct GlobalRoute {
    ControlModule* src;
    ModTargetID targetID;
    float amount;
    ModType type;

    GlobalRoute(ControlModule* s,
                ModTargetID id,
                float amt,
                ModType t)
        : src(s), targetID(id), amount(amt), type(t) {}
};

class Synth {
public:

    static const int NUM_VOICES = 16;
    std::array<Voice, NUM_VOICES> voices;

    std::vector<GlobalRoute> globalRoutes;
    int nextVoiceToSteal = 0;

    Synth();

    void noteOn(int midiNote);
    void noteOff(int midiNote);
    Sample process(float sampleRate);
};
