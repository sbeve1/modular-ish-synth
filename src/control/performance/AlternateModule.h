#pragma once
#include "control/ControlModule.h"

class AlternateModule : public ControlModule {
public:
    float value = 0.0f;
    float process(float) override { return value; }
};
