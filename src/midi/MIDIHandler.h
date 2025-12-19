#pragma once
#include "platform/RtMidi.h"

class Synth; // forward declaration

class MIDIHandler {
public:
    MIDIHandler();

    bool poll(unsigned char& status, unsigned char& note, unsigned char& velocity);

    // MIDI event handlers
    void onNoteOn(int channel, int note, int velocity);
    void onNoteOff(int channel, int note);
    void onControlChange(int cc, int value);
    void onAftertouch(int pressure);
    void onPitchBend(int value14bit);

    Synth* synth = nullptr;  

private:
    RtMidiIn midiin;
};
