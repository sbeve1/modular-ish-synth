#pragma once

#include "core/CONSTANTS.h"

enum class FilterMode : uint8_t {
    LP = 0,
    BP = 1,
    HP = 2
};

struct FilterParams {
    FilterMode mode = FilterMode::LP;
    
    float cutoffBase = 1000.0f;
    float cutoffMod  = 0.0f;

    float resBase    = 0.0f;
    float resMod     = 0.0f;

    float driveBase  = 0.0f;
    float driveMod   = 0.0f;

    float fatBase    = 0.0f;
    float fatMod     = 0.0f;

    float mixBase    = 1.0f;
    float mixMod     = 0.0f;

    float levelBase  = dbToLin(-0.0f);
    float levelMod   = 0.0f;

    float panBase    = 0.0f;
    float panMod     = 0.0f;
};
