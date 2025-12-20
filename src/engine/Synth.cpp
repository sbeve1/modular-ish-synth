#include "Synth.h"
#include <limits>

Synth::Synth()
{
    basicWavetable = WavetableFactory::makeBasicShapes();
}

void Synth::noteOn(int note, float velocity)
{
    Voice* chosen = nullptr;

    // -----------------------------------------
    // 1. Try to find a free voice
    // -----------------------------------------
    for (auto& v : voices) {
        if (!v.active) {
            chosen = &v;
            break;
        }
    }

    // -----------------------------------------
    // 2. Steal the quietest active voice
    // -----------------------------------------
    if (!chosen) {
        float minEnv = std::numeric_limits<float>::max();

        for (auto& v : voices) {
            float env = v.env1.getValue();
            if (env < minEnv) {
                minEnv = env;
                chosen = &v;
            }
        }
    }

    // Safety (should never happen)
    if (!chosen)
        return;

    chosen->start(note, velocity, &basicWavetable);
}

void Synth::noteOff(int note)
{
    for (auto& v : voices) {
        if (v.active && v.midiNote == note) {
            v.stop();
        }
    }
}

Sample Synth::process(float sr)
{
    Sample mix{};

    for (auto& voice : voices) {
        if (!voice.active)
            continue;

        Sample v = voice.process(sr);
        mix.left  += v.left;
        mix.right += v.right;
    }

    return limiter.process(mix);
}
