#include "Synth.h"

Synth::Synth()
{
    basicWavetable = WavetableFactory::makeBasicShapes();
}

void Synth::noteOn(int note, float velocity)
{
    Voice* v = nullptr;
    for (auto& voice : voices) {
        if (!voice.active) { v = &voice; break; }
    }

    if (!v) {
        v = &voices[nextVoice];
        nextVoice = (nextVoice + 1) % NUM_VOICES;
    }

    v->start(note, velocity, &basicWavetable);
}

void Synth::noteOff(int note)
{
    for (auto& v : voices)
        if (v.active && v.midiNote == note)
            v.stop();
}

Sample Synth::process(float sr)
{
    Sample mix{};
    for (auto& v : voices)
        if (v.active)
            mix.left += v.process(sr).left;
            mix.right += v.process(sr).right;

    return limiter.process(mix);
}
