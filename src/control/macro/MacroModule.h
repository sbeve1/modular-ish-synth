#pragma once
#include "control/ControlModule.h"

class MacroModule : public ControlModule {
public:
    float* ptr = nullptr;
    float process(float) override { return *ptr; }
};
