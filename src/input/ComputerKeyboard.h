#pragma once

#include "midi/MidiQueue.h"

// Plays notes from the computer keyboard via console key events, so the synth
// works without a MIDI controller. Runs on the main thread and pushes into its
// own SPSC queue (the MIDI queue already has RtMidi as its producer).
//
//   A W S E D F T G Y H U J K O L   chromatic from C (white keys on the home row)
//   Z / X                           octave down / up
//   1-4                             preset
//   Enter                           quit
class ComputerKeyboard {
public:
    explicit ComputerKeyboard(MidiRing<256>& queue) : queue(queue) {}

    // false if stdin isn't an interactive console
    bool open();

    // Handles pending key events, waiting up to timeoutMs for one.
    // Returns false when the user pressed Enter.
    bool poll(unsigned timeoutMs);

    static void printHelp();

private:
    MidiRing<256>& queue;
    void* input = nullptr;

    int octaveShift = 0;      // in octaves, relative to C4
    int heldNote[256];        // note sent for each held key, -1 if none
    bool isDown[256];         // to ignore auto-repeat

    void keyDown(unsigned short vk);
    void keyUp(unsigned short vk);
};
