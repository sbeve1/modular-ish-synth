#pragma once
#include "ModTargetID.h"
#include "Voice.h"

// A single resolver function for ALL synth parameters
inline float* resolveTarget(Voice& v, ModTargetID id) {
    switch (id) {

        case ModTargetID::VCA_GAIN:
            return v.vca.getGainPtr();

        case ModTargetID::OSC_FREQUENCY:
            return v.osc.getFreqPtr();

        case ModTargetID::FILTER_CUTOFF:
            return v.filter.getCutoffPtr();

        case ModTargetID::FILTER_RESONANCE:
            return v.filter.getResonancePtr(); // implement if needed
    }

    return nullptr;
}
