#pragma once
#include "control/ControlModule.h"

// Simple value-owning NOTE source (key tracking)
class NoteModule : public ControlModule {
public:
    float value = 0.0f;

    float process(float) override {
        return value;
    }
};
