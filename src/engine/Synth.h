#pragma once
#include <array>
#include <vector>
#include "Voice.h"
#include "ModSourceID.h"
#include "ModTargetID.h"


struct GlobalRoute {
    ModSourceID srcID;
    ModTargetID targetID;
    float amount;
    ModType type;

    GlobalRoute(ModSourceID s, ModTargetID t, float amt, ModType ty) {
        srcID = s;
        targetID = t;
        amount = amt;
        type = ty;
    }

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
