#include "Synth.h"

Synth::Synth() {
    // Give voices access to this synth
    for (auto& v : voices)
        v.parentSynth = this;

    // ------- DEFAULT ROUTES -------

    // ENV → AMP
    globalRoutes.emplace_back(
        nullptr,
        resolveVcaGain,
        1.0f,
        ModType::Replace
    );

    // ENV → Filter cutoff
    globalRoutes.emplace_back(
        nullptr,
        resolveFilterCutoff,
        500.0f,
        ModType::Add
    );

    // LFO → Pitch
    globalRoutes.emplace_back(
        nullptr,
        resolveOscFrequency,
        5.0f,
        ModType::Add
    );
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

    // -------------------------------
    // APPLY GLOBAL MODULATION ROUTES
    // -------------------------------
    for (auto& gr : globalRoutes)
    {
        ControlModule* src = nullptr;

        // auto-resolve typical modulation sources
        if (gr.src == nullptr) {
            if (gr.resolveTarget == resolveVcaGain ||
                gr.resolveTarget == resolveFilterCutoff)
                src = &v->env;

            else if (gr.resolveTarget == resolveOscFrequency)
                src = &v->lfo;
        }
        else {
            src = gr.src;
        }

        float* tgt = gr.resolveTarget(*v);
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
