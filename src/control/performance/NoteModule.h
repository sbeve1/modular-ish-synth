#pragma once
#include "control/ControlModule.h"

class NoteModule : public ControlModule {
public:
    float* midiNote = nullptr;    // pointer into Voice
    float process(float) override { return *midiNote; }
};
