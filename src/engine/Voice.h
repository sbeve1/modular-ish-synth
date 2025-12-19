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

#include <iostream>

struct Voice
{
    Oscillator osc;
    VCA        vca;
    Filter     filter;

    ModMatrix mod;

    LFO        lfo;
    Envelope   env;

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
        baseFreq = osc.midiNoteToFreq(note);
        osc.setFrequency(baseFreq);

        velocityMod.value = vel;   // <-- APPLIED HERE
        std::cout << "Velocity = " << vel << std::endl;

        env.noteOn();

        mod.clearAll();

        // Register sources
        mod.registerSource(ModSourceID::ENV1, &env);
        mod.registerSource(ModSourceID::LFO1, &lfo);

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

        // Register targets
        mod.registerTarget(ModTargetID::VCA_GAIN,         vca.getGainPtr());
        mod.registerTarget(ModTargetID::FILTER_CUTOFF,    filter.getCutoffPtr());
        mod.registerTarget(ModTargetID::FILTER_RESONANCE, filter.getResonancePtr());
        mod.registerTarget(ModTargetID::OSC_FREQUENCY,    osc.getFreqPtr());

        active = true;
    }

    void stop() { env.noteOff(); }

    Sample process(float sr)
    {
        if (!active)
            return {0.f, 0.f};

        mod.process(sr);

        Sample signal = osc.process(sr, {0.0f, 0.0f});
        signal = filter.process(sr, signal);
        signal = vca.process(sr, signal);

        if (!env.isActive())
            active = false;

        return signal;
    }
};
