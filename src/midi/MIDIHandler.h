#pragma once
#include "platform/RtMidi.h"
#include <vector>
#include <cstdint>
#include "MidiQueue.h"

class Synth; // forward declaration

class MIDIHandler {
public:
    MIDIHandler();

    // polling interface (kept for compatibility, but we'll use callback)
    bool poll(unsigned char& status, unsigned char& note, unsigned char& velocity);

    // MIDI event handlers (kept)
    void onNoteOn(int channel, int note, int velocity);
    void onNoteOff(int channel, int note);
    void onControlChange(int cc, int value);
    void onAftertouch(int pressure);
    void onPitchBend(int value14bit);

    // SPSC queue wiring
    void setQueue(MidiRing<4096>* q) { queue = q; }

    Synth* synth = nullptr;

private:
    RtMidiIn midiin;
    MidiRing<4096>* queue = nullptr;

    // RtMidi callback
    static void rtCallback(double timeStamp, std::vector<unsigned char> *message, void *userData);
    // helper to enqueue an event
    void enqueueEvent(const MidiEvent& e);
};
