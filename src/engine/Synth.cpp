#include "Synth.h"
#include <cmath>
#include "MasterLimiter.h"

Synth::Synth() {
    // Serum-style velocity → amp
    globalRoutes.emplace_back(
        ModSourceID::VELOCITY,
        ModTargetID::VCA_GAIN,
        1.5f,
        ModType::EXP_CURVE
    );

    // Envelope shaping
    globalRoutes.emplace_back(ModSourceID::ENV1, ModTargetID::VCA_GAIN, 1.0f, ModType::REPLACE);
    globalRoutes.emplace_back(ModSourceID::ENV1, ModTargetID::FILTER_CUTOFF, 500.0f, ModType::ADD);
    globalRoutes.emplace_back(ModSourceID::ENV1, ModTargetID::FILTER_RESONANCE, 0.5f, ModType::ADD);

    // LFO
    globalRoutes.emplace_back(ModSourceID::LFO1, ModTargetID::OSC_FREQUENCY, 5.0f, ModType::ADD);
}

void Synth::noteOn(int midiNote, float velocity)
{
    Voice* v = nullptr;

    for (auto& voice : voices) {
        if (!voice.active) {
            v = &voice;
            break;
        }
    }

    if (!v) {
        v = &voices[nextVoiceToSteal];
        nextVoiceToSteal = (nextVoiceToSteal + 1) % NUM_VOICES;
    }

    v->start(midiNote, velocity);

    v->mod.numRoutes = 0;
    for (auto& gr : globalRoutes)
        v->mod.addRoute(gr.srcID, gr.targetID, gr.amount, gr.type);
}

void Synth::noteOff(int midiNote)
{
    for (auto& v : voices)
        if (v.active && v.midiNote == midiNote)
            v.stop();
}

Sample Synth::process(float sr)
{
    Sample out = {0.f, 0.f};

    int activeVoices = 0;

    for (auto& v : voices) {
        if (v.active) activeVoices++;

        Sample s = v.process(sr);
        out.left  += s.left;
        out.right += s.right;
    }

    out.left  *= MASTER_GAIN;
    out.right *= MASTER_GAIN;

    static MasterLimiter lim;
    out = lim.process(out);
    
    return out;
}
