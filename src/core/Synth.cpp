#include "Synth.h"

void Synth::noteOn(int midiNote) {
    // 1. Use a free voice if available
    for (auto& v : voices) {
        if (!v.active) {
            v.start(midiNote);
            return;
        }
    }

    // 2. No free voices? Steal in round-robin fashion
    voices[nextVoiceToSteal].start(midiNote);

    nextVoiceToSteal++;
    if (nextVoiceToSteal >= NUM_VOICES)
        nextVoiceToSteal = 0;
}

void Synth::noteOff(int midiNote) {
    for (auto& v : voices) {
        if (v.active && v.midiNote == midiNote) {
            v.stop();
        }
    }
}

Sample Synth::process(float sampleRate) {
    Sample out = { 0.0f, 0.0f };

    for (auto& v : voices) {
        Sample voiceSample = v.process(sampleRate);
        out.left += voiceSample.left;
        out.right += voiceSample.right;
    }

    out.left *= 0.2f;
    out.right *= 0.2f; // master gain reduction to prevent clipping
    
    return out;
}
