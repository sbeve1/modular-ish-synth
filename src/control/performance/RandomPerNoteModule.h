#pragma once
#include "control/ControlModule.h"
#include <cstdlib>

class RandomPerNoteModule : public ControlModule {
public:
    float value = 0.0f;
    float process(float) override { return value; }
};
