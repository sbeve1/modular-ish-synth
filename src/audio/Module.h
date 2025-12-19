#pragma once
#include "core/Sample.h"
#include "core/CONSTANTS.h"

class Module {
public:
    virtual ~Module() {}

    // audio-rate processing
    virtual Sample process(float sampleRate, const Sample& in) = 0;
};
