#pragma once
#include "ModTargetID.h"
#include "Voice.h"

inline float* resolveTarget(Voice& v, ModTargetID id) {
    switch (id) {
        case ModTargetID::VCA_GAIN:
            return v.vca.getGainPtr();

        case ModTargetID::FILTER_CUTOFF:
            return v.filter.getCutoffPtr();

        case ModTargetID::FILTER_RESONANCE:
            return v.filter.getResonancePtr();

        case ModTargetID::OSC_FREQUENCY:
            return v.osc.getFreqPtr();

        default:
            return nullptr;
    }
}
