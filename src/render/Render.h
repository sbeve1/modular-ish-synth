#pragma once

#include <vector>

class Synth;

// A scripted note event. velocity <= 0 means note off.
struct NoteEvent {
    double timeSec;
    int note;
    float velocity;
};

// Four-bar Am-F-C-G progression with bass and an arpeggio, played twice.
std::vector<NoteEvent> demoSequence();

// Renders the events through the synth, offline and sample-accurate.
// Returns interleaved stereo frames; length = last event + tailSec.
// The synth must already be prepare()d at sampleRate.
std::vector<float> renderSequence(Synth& synth, float sampleRate,
                                  std::vector<NoteEvent> events, double tailSec);

// Writes interleaved stereo float frames as a 32-bit float WAV.
bool writeWav(const char* path, const std::vector<float>& interleaved, unsigned sampleRate);
