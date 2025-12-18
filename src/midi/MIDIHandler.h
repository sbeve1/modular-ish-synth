#pragma once
#include "RtMidi.h"
#include <vector>

class MIDIHandler {
public:
    MIDIHandler();
    bool poll(unsigned char& status, unsigned char& note, unsigned char& velocity);

private:
    RtMidiIn midiin;
};
