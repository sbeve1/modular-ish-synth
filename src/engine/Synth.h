#pragma once

#include <array>
#include <vector>

#include "Voice.h"
#include "ModSourceID.h"
#include "ModTargetID.h"
#include "ModMatrix.h"
#include "core/Sample.h"

struct GlobalRoute {
    ModSourceID srcID;
    ModTargetID targetID;
    float amount;
    ModType type;

    GlobalRoute(ModSourceID s, ModTargetID t, float amt, ModType ty)
        : srcID(s), targetID(t), amount(amt), type(ty) {}
};

class Synth {
public:
    static constexpr int NUM_VOICES = 16;

    std::array<Voice, NUM_VOICES> voices;
    std::vector<GlobalRoute> globalRoutes;

    int nextVoiceToSteal = 0;

    Synth();

    void noteOn(int midiNote, float velocity);     // FIXED
    void noteOff(int midiNote);
    Sample process(float sampleRate);

    void addModRoute(ModSourceID src, ModTargetID tgt, float amount, ModType type) {
        globalRoutes.emplace_back(src, tgt, amount, type);
    }

    void clearModRoutes() {
        globalRoutes.clear();
    }

    void removeModRoute(int index) {
        if (index >= 0 && index < (int)globalRoutes.size())
            globalRoutes.erase(globalRoutes.begin() + index);
    }

    void setModRouteAmount(int index, float amount) {
        if (index >= 0 && index < (int)globalRoutes.size())
            globalRoutes[index].amount = amount;
    }

    void setModRouteSource(int index, ModSourceID src){
        if (index >= 0 && index < (int)globalRoutes.size())
            globalRoutes[index].srcID = src;
    }

    void setModRouteTarget(int index, ModTargetID tgt){
        if (index >= 0 && index < (int)globalRoutes.size())
            globalRoutes[index].targetID = tgt;
    }

    void setModRouteType(int index, ModType type){
        if (index >= 0 && index < (int)globalRoutes.size())
            globalRoutes[index].type = type;
    }
};
