#pragma once

#include "audio/Oscillator.h"
#include "audio/VCA.h"
#include "audio/Filter.h"

#include "engine/ModMatrix.h"
#include "core/Sample.h"

// Control modules
#include "control/Envelope.h"
#include "control/LFO.h"

#include "control/performance/NoteModule.h"
#include "control/performance/VelocityModule.h"
#include "control/performance/ModWheelModule.h"
#include "control/performance/AftertouchModule.h"
#include "control/performance/PitchBendModule.h"
#include "control/performance/AlternateModule.h"
#include "control/performance/RandomPerVoiceModule.h"
#include "control/performance/RandomPerNoteModule.h"

#include "control/macro/MacroModule.h"
#include "control/macro/MacroStorage.h"


struct Voice
{
    // audio processors
    Oscillator osc;
    VCA        vca;
    Filter     filter;

    // modulation routing
    ModMatrix mod;

    // control modules
    LFO        lfo1, lfo2, lfo3, lfo4;
    Envelope   env1, env2, env3;

    // performance + macro modules
    NoteModule noteMod;
    VelocityModule velocityMod;
    ModWheelModule modWheelMod;
    AftertouchModule aftertouchMod;
    PitchBendModule pitchBendMod;
    AlternateModule alternateMod;
    RandomPerVoiceModule randVoiceMod;
    RandomPerNoteModule randNoteMod;

    MacroStorage macros;
    MacroModule macro1Mod, macro2Mod, macro3Mod, macro4Mod;

    bool active = false;
    int midiNote = -1;
    float baseFreq = 0.0f;

    void start(int note, float vel)
    {
        midiNote = note;

        // Set oscillator frequency
        baseFreq = osc.midiNoteToFreq(note);
        osc.setFrequency(baseFreq);

        // Apply phase offset (IMPORTANT!)
        osc.phase = osc.phaseOffsetBase * TWO_PI;

        // Set filter keytracking reference
        filter.currentNote = note;

        // trigger envelopes
        env1.noteOn(note, vel);
        env2.noteOn(note, vel);
        env3.noteOn(note, vel);

        // reset LFOs
        lfo1.retrigger();
        lfo2.retrigger();
        lfo3.retrigger();
        lfo4.retrigger();

        // reset mod matrix (clears routes and sources)
        mod.clearAll();

        // ============================
        // REGISTER SOURCES
        // ============================
        mod.registerSource(ModSourceID::ENV1, &env1);
        mod.registerSource(ModSourceID::ENV2, &env2);
        mod.registerSource(ModSourceID::ENV3, &env3);

        mod.registerSource(ModSourceID::LFO1, &lfo1);
        mod.registerSource(ModSourceID::LFO2, &lfo2);
        mod.registerSource(ModSourceID::LFO3, &lfo3);
        mod.registerSource(ModSourceID::LFO4, &lfo4);

        noteMod.midiNote = (float*)&midiNote;
        mod.registerSource(ModSourceID::NOTE, &noteMod);

        mod.registerSource(ModSourceID::VELOCITY, &velocityMod);
        mod.registerSource(ModSourceID::MOD_WHEEL, &modWheelMod);
        mod.registerSource(ModSourceID::AFTERTOUCH, &aftertouchMod);
        mod.registerSource(ModSourceID::PITCH_BEND, &pitchBendMod);

        alternateMod.value = (alternateMod.value == 0.0f ? 1.0f : 0.0f);
        mod.registerSource(ModSourceID::ALTRNATE, &alternateMod);

        randVoiceMod.value = (float)rand() / RAND_MAX;
        randNoteMod.value  = (float)rand() / RAND_MAX;
        mod.registerSource(ModSourceID::RANDOM_PER_VOICE, &randVoiceMod);
        mod.registerSource(ModSourceID::RANDOM_PER_NOTE, &randNoteMod);

        macro1Mod.ptr = &macros.m1;
        macro2Mod.ptr = &macros.m2;
        macro3Mod.ptr = &macros.m3;
        macro4Mod.ptr = &macros.m4;
        mod.registerSource(ModSourceID::MACRO1, &macro1Mod);
        mod.registerSource(ModSourceID::MACRO2, &macro2Mod);
        mod.registerSource(ModSourceID::MACRO3, &macro3Mod);
        mod.registerSource(ModSourceID::MACRO4, &macro4Mod);

        // ============================
        // REGISTER TARGETS
        // ============================
        // NOTE: registerTarget now takes (modShadowPtr, baseValue).
        // Modules must expose a mod shadow pointer and base knob values.

        // VCA
        mod.registerTarget(ModTargetID::VCA_GAIN, vca.getGainModPtr(), vca.getGainBase());
        mod.registerTarget(ModTargetID::VCA_PAN,  vca.getPanModPtr(),  vca.getPanBase());

        // Filter
        mod.registerTarget(ModTargetID::FILTER_CUTOFF,     filter.getCutoffModPtr(),     filter.getCutoffBase());
        mod.registerTarget(ModTargetID::FILTER_RESONANCE,  filter.getResonanceModPtr(),  filter.getResonanceBase());
        mod.registerTarget(ModTargetID::FILTER_DRIVE,      filter.getDriveModPtr(),      filter.getDriveBase());
        mod.registerTarget(ModTargetID::FILTER_POST_DRIVE, filter.getPostDriveModPtr(),  filter.getPostDriveBase());
        mod.registerTarget(ModTargetID::FILTER_MIX,        filter.getMixModPtr(),        filter.getMixBase());
        mod.registerTarget(ModTargetID::FILTER_KEYTRACK,   filter.getKeytrackModPtr(),   filter.getKeytrackBase());

        // Oscillator
        mod.registerTarget(ModTargetID::OSC_FREQUENCY, osc.getFreqModPtr(),       osc.getFreqBase());
        mod.registerTarget(ModTargetID::OSC_FINE,      osc.getFineModPtr(),       osc.getFineBase());
        mod.registerTarget(ModTargetID::OSC_LEVEL,     osc.getLevelModPtr(),      osc.getLevelBase());
        mod.registerTarget(ModTargetID::OSC_PHASE_OFF, osc.getPhaseOffsetModPtr(),osc.getPhaseOffsetBase());
        mod.registerTarget(ModTargetID::OSC_PAN,       osc.getPanModPtr(),        osc.getPanBase());

        // Envelopes (we modulate the parameter values directly)
        mod.registerTarget(ModTargetID::ENV1_ATTACK,  &env1.attack,  env1.attack);
        mod.registerTarget(ModTargetID::ENV1_HOLD,    &env1.hold,    env1.hold);
        mod.registerTarget(ModTargetID::ENV1_DECAY,   &env1.decay,   env1.decay);
        mod.registerTarget(ModTargetID::ENV1_SUSTAIN, &env1.sustain, env1.sustain);
        mod.registerTarget(ModTargetID::ENV1_RELEASE, &env1.release, env1.release);
        mod.registerTarget(ModTargetID::ENV1_AMOUNT,  &env1.amount,  env1.amount);

        mod.registerTarget(ModTargetID::ENV2_ATTACK,  &env2.attack,  env2.attack);
        mod.registerTarget(ModTargetID::ENV2_HOLD,    &env2.hold,    env2.hold);
        mod.registerTarget(ModTargetID::ENV2_DECAY,   &env2.decay,   env2.decay);
        mod.registerTarget(ModTargetID::ENV2_SUSTAIN, &env2.sustain, env2.sustain);
        mod.registerTarget(ModTargetID::ENV2_RELEASE, &env2.release, env2.release);
        mod.registerTarget(ModTargetID::ENV2_AMOUNT,  &env2.amount,  env2.amount);

        mod.registerTarget(ModTargetID::ENV3_ATTACK,  &env3.attack,  env3.attack);
        mod.registerTarget(ModTargetID::ENV3_HOLD,    &env3.hold,    env3.hold);
        mod.registerTarget(ModTargetID::ENV3_DECAY,   &env3.decay,   env3.decay);
        mod.registerTarget(ModTargetID::ENV3_SUSTAIN, &env3.sustain, env3.sustain);
        mod.registerTarget(ModTargetID::ENV3_RELEASE, &env3.release, env3.release);
        mod.registerTarget(ModTargetID::ENV3_AMOUNT,  &env3.amount,  env3.amount);

        // LFOs
        mod.registerTarget(ModTargetID::LFO1_RATE,     lfo1.getFrequencyPtr(),    lfo1.freq);
        mod.registerTarget(ModTargetID::LFO1_BPM,      lfo1.getBPMPtr(),          lfo1.bpm);
        mod.registerTarget(ModTargetID::LFO1_SYNC_DIV, lfo1.getSyncDivisionPtr(), lfo1.syncDivision);
        mod.registerTarget(ModTargetID::LFO1_DELAY,    lfo1.getDelayPtr(),        lfo1.delay);
        mod.registerTarget(ModTargetID::LFO1_RISE,     lfo1.getRisePtr(),         lfo1.rise);
        mod.registerTarget(ModTargetID::LFO1_AMOUNT,   lfo1.getAmountPtr(),       lfo1.amount);

        mod.registerTarget(ModTargetID::LFO2_RATE,     lfo2.getFrequencyPtr(),    lfo2.freq);
        mod.registerTarget(ModTargetID::LFO2_BPM,      lfo2.getBPMPtr(),          lfo2.bpm);
        mod.registerTarget(ModTargetID::LFO2_SYNC_DIV, lfo2.getSyncDivisionPtr(), lfo2.syncDivision);
        mod.registerTarget(ModTargetID::LFO2_DELAY,    lfo2.getDelayPtr(),        lfo2.delay);
        mod.registerTarget(ModTargetID::LFO2_RISE,     lfo2.getRisePtr(),         lfo2.rise);
        mod.registerTarget(ModTargetID::LFO2_AMOUNT,   lfo2.getAmountPtr(),       lfo2.amount);

        mod.registerTarget(ModTargetID::LFO3_RATE,     lfo3.getFrequencyPtr(),    lfo3.freq);
        mod.registerTarget(ModTargetID::LFO3_BPM,      lfo3.getBPMPtr(),          lfo3.bpm);
        mod.registerTarget(ModTargetID::LFO3_SYNC_DIV, lfo3.getSyncDivisionPtr(), lfo3.syncDivision);
        mod.registerTarget(ModTargetID::LFO3_DELAY,    lfo3.getDelayPtr(),        lfo3.delay);
        mod.registerTarget(ModTargetID::LFO3_RISE,     lfo3.getRisePtr(),         lfo3.rise);
        mod.registerTarget(ModTargetID::LFO3_AMOUNT,   lfo3.getAmountPtr(),       lfo3.amount);

        mod.registerTarget(ModTargetID::LFO4_RATE,     lfo4.getFrequencyPtr(),    lfo4.freq);
        mod.registerTarget(ModTargetID::LFO4_BPM,      lfo4.getBPMPtr(),          lfo4.bpm);
        mod.registerTarget(ModTargetID::LFO4_SYNC_DIV, lfo4.getSyncDivisionPtr(), lfo4.syncDivision);
        mod.registerTarget(ModTargetID::LFO4_DELAY,    lfo4.getDelayPtr(),        lfo4.delay);
        mod.registerTarget(ModTargetID::LFO4_RISE,     lfo4.getRisePtr(),         lfo4.rise);
        mod.registerTarget(ModTargetID::LFO4_AMOUNT,   lfo4.getAmountPtr(),       lfo4.amount);

        active = true;
    }

    void stop()
    {
        env1.noteOff();
        env2.noteOff();
        env3.noteOff();
    }

    Sample process(float sr)
    {
        if (!active)
            return { 0.f, 0.f };

        // run modulation (writes into mod-shadows)
        mod.process(sr);

        // audio chain uses the mod-shadows inside modules
        Sample signal = osc.process(sr, {0.f, 0.f});
        signal = filter.process(sr, signal);
        signal = vca.process(sr, signal);

        // normalize voice
        signal.left  *= VOICE_GAIN_NORM;
        signal.right *= VOICE_GAIN_NORM;

        // stop voice when AMP envelope is done
        if (!env1.isActive())
            active = false;

        return signal;
    }
};
