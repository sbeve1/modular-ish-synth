#pragma once
#include <cstdint>

struct VoiceState {
    bool active = false;
    bool released = false;

    int midiNote = -1;
    int noteId   = -1;

    uint32_t samplesSinceNoteOn  = 0;
    uint32_t samplesSinceNoteOff = 0;

    inline void noteOn(int note) {
        active = true;
        released = false;
        midiNote = note;
        samplesSinceNoteOn = 0;
        samplesSinceNoteOff = 0;
    }

    inline void noteOff() {
        released = true;
        samplesSinceNoteOff = 0;
    }
};
