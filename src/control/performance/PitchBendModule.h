#pragma once
#include "control/ControlModule.h"

class PitchBendModule : public ControlModule {
public:
    float value = 0.0f;           // -1 to +1
    float process(float) override { return value; }
};
