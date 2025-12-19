#pragma once

#include "audio/Oscillator.h"
#include "audio/VCA.h"
#include "audio/Filter.h"
#include "control/Envelope.h"
#include "control/LFO.h"

#include "ModMatrix.h"
#include "core/Sample.h"

struct Voice
{
    Oscillator osc;
    Envelope   env;
    VCA        vca;
    Filter     filter;
    LFO        lfo;

    ModMatrix mod;

    bool active = false;
    int midiNote = -1;
    float baseFreq = 0.0f;

    void start(int note)
    {
        midiNote = note;
        baseFreq = osc.midiNoteToFreq(note);
        osc.setFrequency(baseFreq);

        env.noteOn();

        // ---- SERUM STYLE SOURCE REGISTRATION ----
        mod.clearAll();
        mod.registerSource(ModSourceID::ENV1, &env);
        mod.registerSource(ModSourceID::LFO1, &lfo);

        // ---- SERUM STYLE TARGET REGISTRATION ----
        mod.registerTarget(ModTargetID::VCA_GAIN,         vca.getGainPtr());
        mod.registerTarget(ModTargetID::FILTER_CUTOFF,    filter.getCutoffPtr());
        mod.registerTarget(ModTargetID::FILTER_RESONANCE, filter.getResonancePtr());
        mod.registerTarget(ModTargetID::OSC_FREQUENCY,    osc.getFreqPtr());

        active = true;
    }


    void stop()
    {
        env.noteOff();
    }

    Sample process(float sr)
    {
        if (!active)
            return {0.f, 0.f};

        mod.process(sr);

        // FIXED AUDIO CHAIN (Serum-style)
        Sample signal = osc.process(sr, {0.0f, 0.0f});
        signal = filter.process(sr, signal);
        signal = vca.process(sr, signal);

        if (!env.isActive())
            active = false;

        return signal;
    }
};
