#pragma once
#include "control/ControlModule.h"

class VelocityModule : public ControlModule {
public:
    float value = 1.0f;           // 0–1
    float process(float) override { return value; }
};
