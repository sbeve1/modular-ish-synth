#pragma once
#include "ModSourceID.h"
#include "Voice.h"

inline ControlModule* resolveSource(Voice& v, ModSourceID id) {
    switch (id) {
        case ModSourceID::ENV1:
            return &v.env;
        case ModSourceID::LFO1:
            return &v.lfo;
        default:
            return nullptr;
    }
}
