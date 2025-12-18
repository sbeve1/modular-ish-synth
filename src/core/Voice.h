#pragma once

#include "audio/Oscillator.h"
#include "audio/Envelope.h"
#include "audio/VCA.h"
#include "audio/Filter.h"
#include "audio/LFO.h"
#include "ModMatrix.h"          // ← your new Serum-style matrix
#include "core/Sample.h"

#include <cmath>
#include <vector>

struct Voice {
    Oscillator osc;
    Envelope   env;
    VCA        vca;
    Filter     filter;
    LFO        lfo;

    ModMatrix mod;
    std::vector<Module*> audioChain;

    bool  active = false;
    float baseFreq = 0.0f;
    float midiNote = 0.0f;

    // ---------------------------
    //     NOTE ON  
    // ---------------------------
    void start(int note) {
        midiNote = note;
        active = true;

        // Convert MIDI note → base frequency
        baseFreq = osc.midiNoteToFreq(midiNote);
        osc.setFrequency(baseFreq);

        // Restart envelope
        env.noteOn();

        // Audio chain: OSC → FILTER → VCA
        audioChain = { &osc, &filter, &vca };

        // Clear old modulation routes
        mod.clear();

        // ---------------------------
        //   DEFAULT MOD ROUTES
        // ---------------------------

        // 1) Envelope → VCA  (absolute control: Replace)
        mod.addRoute(&env, vca.getGainPtr(), 1.0f, ModType::Replace);

        // 2) Envelope → Filter cutoff (Additive)
        //     Push cutoff up by +500 Hz at env = 1.0f
        mod.addRoute(&env, filter.getCutoffPtr(), 500.0f, ModType::Add);

        // 3) LFO → Oscillator Frequency  (Additive vibrato)
        float semitoneHz = (baseFreq * SEMITONE_FREQ_COEFF) - baseFreq;
        float vibratoDepth = 0.5f;               // 0..1 = fraction of 1 semitone
        float vibratoAmount = semitoneHz * vibratoDepth;
        mod.addRoute(&lfo, osc.getFreqPtr(), vibratoAmount, ModType::Add);
    }


    // ---------------------------
    //     NOTE OFF
    // ---------------------------
    void stop() {
        env.noteOff();
    }


    // ---------------------------
    //     AUDIO PROCESS
    // ---------------------------
    Sample process(float sr) {
        if (!active)
            return {0.0f, 0.0f};

        // 1) Control-rate processing: update modulation targets
        mod.process(sr);

        // 2) Audio-rate processing
        Sample s = {0.0f, 0.0f};
        for (Module* m : audioChain)
            s = m->process(sr, s);

        // 3) Voice termination logic
        //
        // env.isActive() becomes false ONLY when the envelope has fully finished.
        // We also check the VCA gain and audio amplitude to ensure clean cutoff.
        const float eps = 1e-5f;

        if (!env.isActive()) {
            // Gain at zero?
            float g = vca.getGain();   // You can add a getter: float getGain() const { return gain; }

            if (std::abs(g) <= eps &&
                std::abs(s.left)  <= eps &&
                std::abs(s.right) <= eps) 
            {
                active = false;
            }
        }

        return s;
    }
};
