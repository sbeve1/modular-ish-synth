#pragma once

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Wavetable.h"
#include "engine/ModMatrix.h"
#include "engine/VoiceSources.h"

// Control
#include "control/Envelope.h"
#include "control/LFO.h"

struct Voice {
    bool active = false;
    int  midiNote = -1;

    Oscillator oscA;
    Oscillator oscB;
    Oscillator oscC;
    Oscillator sub;
    Noise      noise;

    Envelope env1;
    LFO      lfo1;

    ModMatrix mod;

    void start(int note, float velocity, const Wavetable* wt)
    {
        active   = true;
        midiNote = note;

        // ----------------------------
        // ASSIGN WAVETABLE (CRITICAL)
        // ----------------------------
        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        float f = oscA.midiNoteToFreq(note);
        oscA.freqBase = f;
        oscB.freqBase = f;
        oscC.freqBase = f;
        sub.freqBase  = f;
        sub.semitoneBase = -12;

        env1.noteOn(note, velocity);
        lfo1.retrigger();

        oscA.resetModValues();
        oscB.resetModValues();
        oscC.resetModValues();
        sub.resetModValues();

        mod.clearAll();

        // ----------------------------
        // MOD SOURCES
        // ----------------------------
        mod.registerSource(ModSourceID::ENV1, &env1);
        mod.registerSource(ModSourceID::LFO1, &lfo1);

        // ----------------------------
        // WAVETABLE POSITION TARGETS
        // ----------------------------
        mod.registerTarget(ModTargetID::OSC_A_WT_POS,
                           oscA.getPositionPtr(),
                           oscA.getPositionBase());

        mod.registerTarget(ModTargetID::OSC_B_WT_POS,
                           oscB.getPositionPtr(),
                           oscB.getPositionBase());

        mod.registerTarget(ModTargetID::OSC_C_WT_POS,
                           oscC.getPositionPtr(),
                           oscC.getPositionBase());

        mod.registerTarget(ModTargetID::SUB_WT_POS,
                           sub.getPositionPtr(),
                           sub.getPositionBase());
    }

    void stop()
    {
        env1.noteOff();
    }

    VoiceSources processSources(float sr)
    {
        VoiceSources out{};
        if (!active) return out;

        mod.process(sr);

        out.oscA  = oscA.process(sr, {});
        out.oscB  = oscB.process(sr, {});
        out.oscC  = oscC.process(sr, {});
        out.sub   = sub.process(sr, {});
        out.noise = noise.process(sr, {});

        if (!env1.isActive())
            active = false;

        return out;
    }

    float envValue(float sr)
    {
        return env1.process(sr);
    }
};
