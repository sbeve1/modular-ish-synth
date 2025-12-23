#include "Synth.h"
#include <limits>

Synth::Synth()
{
    basicWavetable = WavetableFactory::makeBasicShapes();
}

void Synth::noteOn(int note, float velocity)
{
    Voice* chosen = nullptr;

    // 1) Find a free voice
    for (auto& v : voices) {
        if (!v.state.active) {
            chosen = &v;
            break;
        }
    }

    // 2) Steal the oldest voice if all are active
    if (!chosen) {
        int oldestId = std::numeric_limits<int>::max();
        for (auto& v : voices) {
            if (v.state.noteId < oldestId) {
                oldestId = v.state.noteId;
                chosen = &v;
            }
        }
    }

    if (!chosen)
        return;

    chosen->state.noteId = nextNoteId++;
    chosen->start(note, velocity, &basicWavetable);
}

void Synth::noteOff(int note)
{
    // Release only the most recent voice for this note
    int bestId = -1;
    Voice* chosen = nullptr;

    for (auto& v : voices) {
        if (v.state.active && v.state.midiNote == note) {
            if (!chosen || v.state.noteId > bestId) {
                bestId = v.state.noteId;
                chosen = &v;
            }
        }
    }

    if (chosen)
        chosen->stop();
}

Sample Synth::process(float sr)
{
    Sample mix{};

    for (auto& v : voices) {
        if (!v.state.active)
            continue;

        Sample s = v.process(sr);

        mix.left  += s.left  * VOICE_GAIN_NORM;
        mix.right += s.right * VOICE_GAIN_NORM;
    }

    mix.left  *= MASTER_GAIN;
    mix.right *= MASTER_GAIN;

    return mix;
}
