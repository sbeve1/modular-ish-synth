#pragma once

class ControlModule {
public:
    virtual ~ControlModule() {}

    // control-rate processing: return a float modulation signal
    virtual float process(float sampleRate) = 0;
};
