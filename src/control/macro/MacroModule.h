#pragma once
#include "control/ControlModule.h"
#include <cassert>

// MacroModule is a tiny wrapper that reads a float pointer.
// In debug builds we assert to fail-fast; in release we fallback to 0.0f.
class MacroModule : public ControlModule {
public:
    float* ptr = nullptr;
    float process(float) override {
#ifndef NDEBUG
        assert(ptr && "MacroModule::ptr not set");
#endif
        return ptr ? *ptr : 0.0f;
    }
};
