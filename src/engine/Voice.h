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
        osc.phase = osc.phaseOffset * TWO_PI;

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

        // reset mod matrix
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

        // VCA
        mod.registerTarget(ModTargetID::VCA_GAIN, vca.getGainPtr());
        mod.registerTarget(ModTargetID::VCA_PAN,  vca.getPanPtr());

        // Filter
        mod.registerTarget(ModTargetID::FILTER_CUTOFF,     filter.getCutoffPtr());
        mod.registerTarget(ModTargetID::FILTER_RESONANCE,  filter.getResonancePtr());
        mod.registerTarget(ModTargetID::FILTER_DRIVE,      filter.getDrivePtr());
        mod.registerTarget(ModTargetID::FILTER_POST_DRIVE, filter.getPostDrivePtr());
        mod.registerTarget(ModTargetID::FILTER_MIX,         filter.getMixPtr());
        mod.registerTarget(ModTargetID::FILTER_KEYTRACK,    filter.getKeytrackPtr());

        // Oscillator
        mod.registerTarget(ModTargetID::OSC_FREQUENCY, osc.getFreqPtr());
        mod.registerTarget(ModTargetID::OSC_FINE,      osc.getFinePtr());
        mod.registerTarget(ModTargetID::OSC_LEVEL,     osc.getLevelPtr());
        mod.registerTarget(ModTargetID::OSC_PHASE_OFF, osc.getPhaseOffsetPtr());
        mod.registerTarget(ModTargetID::OSC_PAN,       osc.getPanPtr());

        // Envelopes
        // env1
        mod.registerTarget(ModTargetID::ENV1_ATTACK,  &env1.attack);
        mod.registerTarget(ModTargetID::ENV1_HOLD,    &env1.hold);
        mod.registerTarget(ModTargetID::ENV1_DECAY,   &env1.decay);
        mod.registerTarget(ModTargetID::ENV1_SUSTAIN, &env1.sustain);
        mod.registerTarget(ModTargetID::ENV1_RELEASE, &env1.release);
        mod.registerTarget(ModTargetID::ENV1_AMOUNT,  &env1.amount);

        // env2
        mod.registerTarget(ModTargetID::ENV2_ATTACK,  &env2.attack);
        mod.registerTarget(ModTargetID::ENV2_HOLD,    &env2.hold);
        mod.registerTarget(ModTargetID::ENV2_DECAY,   &env2.decay);
        mod.registerTarget(ModTargetID::ENV2_SUSTAIN, &env2.sustain);
        mod.registerTarget(ModTargetID::ENV2_RELEASE, &env2.release);
        mod.registerTarget(ModTargetID::ENV2_AMOUNT,  &env2.amount);

        // env3
        mod.registerTarget(ModTargetID::ENV3_ATTACK,  &env3.attack);
        mod.registerTarget(ModTargetID::ENV3_HOLD,    &env3.hold);
        mod.registerTarget(ModTargetID::ENV3_DECAY,   &env3.decay);
        mod.registerTarget(ModTargetID::ENV3_SUSTAIN, &env3.sustain);
        mod.registerTarget(ModTargetID::ENV3_RELEASE, &env3.release);
        mod.registerTarget(ModTargetID::ENV3_AMOUNT,  &env3.amount);

        // LFOs
        mod.registerTarget(ModTargetID::LFO1_RATE,     lfo1.getFrequencyPtr());
        mod.registerTarget(ModTargetID::LFO1_BPM,      lfo1.getBPMPtr());
        mod.registerTarget(ModTargetID::LFO1_SYNC_DIV, lfo1.getSyncDivisionPtr());
        mod.registerTarget(ModTargetID::LFO1_DELAY,    lfo1.getDelayPtr());
        mod.registerTarget(ModTargetID::LFO1_RISE,     lfo1.getRisePtr());
        mod.registerTarget(ModTargetID::LFO1_AMOUNT,   lfo1.getAmountPtr());

        mod.registerTarget(ModTargetID::LFO2_RATE,     lfo2.getFrequencyPtr());
        mod.registerTarget(ModTargetID::LFO2_BPM,      lfo2.getBPMPtr());
        mod.registerTarget(ModTargetID::LFO2_SYNC_DIV, lfo2.getSyncDivisionPtr());
        mod.registerTarget(ModTargetID::LFO2_DELAY,    lfo2.getDelayPtr());
        mod.registerTarget(ModTargetID::LFO2_RISE,     lfo2.getRisePtr());
        mod.registerTarget(ModTargetID::LFO2_AMOUNT,   lfo2.getAmountPtr());

        mod.registerTarget(ModTargetID::LFO3_RATE,     lfo3.getFrequencyPtr());
        mod.registerTarget(ModTargetID::LFO3_BPM,      lfo3.getBPMPtr());
        mod.registerTarget(ModTargetID::LFO3_SYNC_DIV, lfo3.getSyncDivisionPtr());
        mod.registerTarget(ModTargetID::LFO3_DELAY,    lfo3.getDelayPtr());
        mod.registerTarget(ModTargetID::LFO3_RISE,     lfo3.getRisePtr());
        mod.registerTarget(ModTargetID::LFO3_AMOUNT,   lfo3.getAmountPtr());

        mod.registerTarget(ModTargetID::LFO4_RATE,     lfo4.getFrequencyPtr());
        mod.registerTarget(ModTargetID::LFO4_BPM,      lfo4.getBPMPtr());
        mod.registerTarget(ModTargetID::LFO4_SYNC_DIV, lfo4.getSyncDivisionPtr());
        mod.registerTarget(ModTargetID::LFO4_DELAY,    lfo4.getDelayPtr());
        mod.registerTarget(ModTargetID::LFO4_RISE,     lfo4.getRisePtr());
        mod.registerTarget(ModTargetID::LFO4_AMOUNT,   lfo4.getAmountPtr());

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

        // run modulation
        mod.process(sr);

        // audio chain
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
