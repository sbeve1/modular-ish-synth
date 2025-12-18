#pragma once
#include <array>
#include "Voice.h"

class Synth {
public:
    static const int NUM_VOICES = 16;
    std::array<Voice, NUM_VOICES> voices;

    int nextVoiceToSteal = 0;

    void noteOn(int midiNote);
    void noteOff(int midiNote);
    Sample process(float sampleRate);
};
