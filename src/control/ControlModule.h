#pragma once

class ControlModule {
public:
    virtual ~ControlModule() = default;

    // Advance internal state ONCE per sample
    virtual float process(float sampleRate) = 0;

    // Read last computed value (does NOT advance state)
    float getValue() const { return value; }

protected:
    float value = 0.0f;
};
