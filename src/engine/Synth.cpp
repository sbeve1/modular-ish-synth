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
    Sample direct{};

    for (auto& voice : voices) {
        if (!voice.active)
            continue;

        Sample v = voice.process(sr);

        // MAIN goes through FX (later)
        mix.left  += v.left;
        mix.right += v.right;
    }

    // FX would go here (later)

    Sample out;
    out.left  = mix.left;
    out.right = mix.right;

    return limiter.process(out);
}


