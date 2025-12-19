#pragma once
#include "control/ControlModule.h"

class AftertouchModule : public ControlModule {
public:
    float value = 0.0f;           // 0–1
    float process(float) override { return value; }
};
