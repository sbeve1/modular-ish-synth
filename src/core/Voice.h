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

    bool active = false;
    int midiNote = -1;
    float baseFreq = 0.0f;

    void start(int note) {
        midiNote = note;
        active = true;

        baseFreq = osc.midiNoteToFreq(note);
        osc.setFrequency(baseFreq);

        env.noteOn();

        audioChain = { &osc, &filter, &vca };

        mod.clear();   // synth will fill routes AFTER this
    }

    void stop() {
        env.noteOff();
    }

    Sample process(float sr) {
        if (!active) return {0, 0};

        mod.process(sr);

        Sample s = {0, 0};
        for (Module* m : audioChain)
            s = m->process(sr, s);

        if (!env.isActive())
            active = false;

        return s;
    }
};
