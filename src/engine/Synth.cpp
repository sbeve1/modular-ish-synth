#include "Synth.h"
#include "ModSourceResolver.h"
#include "ModTargetResolver.h"

Synth::Synth() {
    // ------- DEFAULT ROUTES -------
    globalRoutes.emplace_back(ModSourceID::ENV1,
                            ModTargetID::VCA_GAIN,
                            1.0f,
                            ModType::REPLACE);

    globalRoutes.emplace_back(ModSourceID::ENV1,
                            ModTargetID::FILTER_CUTOFF,
                            500.0f,
                            ModType::ADD);

    globalRoutes.emplace_back(ModSourceID::ENV1,
                            ModTargetID::FILTER_RESONANCE,
                            0.5f,
                            ModType::ADD);

    globalRoutes.emplace_back(ModSourceID::LFO1,
                            ModTargetID::OSC_FREQUENCY,
                            5.0f,
                            ModType::ADD);



}

void Synth::noteOn(int midiNote) {
    Voice* v = nullptr;

    // Find a free voice
    for (auto& voice : voices) {
        if (!voice.active) {
            v = &voice;
            break;
        }
    }

    // Steal a voice if none free
    if (!v) {
        v = &voices[nextVoiceToSteal];
        nextVoiceToSteal = (nextVoiceToSteal + 1) % NUM_VOICES;
    }

    v->start(midiNote);

    // APPLY GLOBAL MODULATION ROUTES
    for (auto& gr : globalRoutes) {

        ControlModule* src = resolveSource(*v, gr.srcID);
        float* tgt = resolveTarget(*v, gr.targetID);

        if (src && tgt)
            v->mod.addRoute(src, tgt, gr.amount, gr.type);
    }

}

void Synth::noteOff(int midiNote) {
    for (auto& v : voices) {
        if (v.active && v.midiNote == midiNote) {
            v.stop();
        }
    }
}

Sample Synth::process(float sampleRate) {
    Sample out = { 0, 0 };

    for (auto& v : voices) {
        Sample s = v.process(sampleRate);
        out.left  += s.left;
        out.right += s.right;
    }

    // master gain
    out.left  *= 0.2f;
    out.right *= 0.2f;

    return out;
}
