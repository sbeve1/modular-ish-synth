#pragma once

#include <vector>
#include <cmath>

#include "audio/Oscillator.h"
#include "audio/Envelope.h"
#include "audio/VCA.h"
#include "audio/Filter.h"
#include "audio/LFO.h"
#include "ModMatrix.h"
#include "core/Sample.h"

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

    void start(int note) {
        midiNote = note;
        active = true;

        baseFreq = osc.midiNoteToFreq(midiNote);
        osc.setFrequency(baseFreq);

        env.noteOn();

        audioChain = { &osc, &filter, &vca };

        mod.clear();  // BUT DO NOT APPLY ROUTES HERE
    }

    void stop() {
        env.noteOff();
    }

    Sample process(float sr) {
        if (!active)
            return {0.0f, 0.0f};

        mod.process(sr);

        Sample s = {0.0f, 0.0f};
        for (Module* m : audioChain)
            s = m->process(sr, s);

        const float eps = 1e-5f;
        if (!env.isActive()) {
            float g = vca.getGain();
            if (fabs(g) <= eps &&
                fabs(s.left) <= eps &&
                fabs(s.right) <= eps)
                active = false;
        }

        return s;
    }
};
