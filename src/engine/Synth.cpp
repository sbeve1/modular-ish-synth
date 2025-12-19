#include "Synth.h"

Synth::Synth() {
    // ------- DEFAULT ROUTES (Serum-style preset init) -------
    globalRoutes.emplace_back(
        ModSourceID::ENV1,
        ModTargetID::VCA_GAIN,
        1.0f,
        ModType::REPLACE
    );

    globalRoutes.emplace_back(
        ModSourceID::ENV1,
        ModTargetID::FILTER_CUTOFF,
        500.0f,
        ModType::ADD
    );

    globalRoutes.emplace_back(
        ModSourceID::ENV1,
        ModTargetID::FILTER_RESONANCE,
        0.5f,
        ModType::ADD
    );

    globalRoutes.emplace_back(
        ModSourceID::LFO1,
        ModTargetID::OSC_FREQUENCY,
        5.0f,
        ModType::ADD
    );
}

void Synth::noteOn(int midiNote)
{
    Voice* v = nullptr;

    // find a free voice
    for (auto& voice : voices) {
        if (!voice.active) {
            v = &voice;
            break;
        }
    }

    // or steal one
    if (!v) {
        v = &voices[nextVoiceToSteal];
        nextVoiceToSteal = (nextVoiceToSteal + 1) % NUM_VOICES;
    }

    // Start voice (this registers modulators + targets inside mod)
    v->start(midiNote);

    v->mod.numRoutes = 0;

    for (auto& gr : globalRoutes) {
        v->mod.addRoute(gr.srcID, gr.targetID, gr.amount, gr.type);
    }
}

void Synth::noteOff(int midiNote)
{
    for (auto& v: voices) {
        if (v.active && v.midiNote == midiNote)
            v.stop();
    }
}

Sample Synth::process(float sampleRate)
{
    Sample out = {0.f, 0.f};

    for (auto& v : voices) {
        Sample s = v.process(sampleRate);
        out.left  += s.left;
        out.right += s.right;
    }

    out.left  *= 0.2f;
    out.right *= 0.2f;

    return out;
}
