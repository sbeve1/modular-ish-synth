#pragma once

#include <array>
#include <cstdlib>

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Wavetable.h"
#include "audio/Filter.h"

#include "engine/ModMatrix.h"
#include "engine/VoiceSources.h"
#include "engine/OscParams.h"
#include "engine/NoiseParams.h"
#include "engine/FilterParams.h"

#include "control/Envelope.h"
#include "control/LFO.h"

#include "control/performance/AftertouchModule.h"
#include "control/performance/AlternateModule.h"
#include "control/performance/ModWheelModule.h"
#include "control/performance/NoteModule.h"
#include "control/performance/PitchBendModule.h"
#include "control/performance/RandomPerNoteModule.h"
#include "control/performance/RandomPerVoiceModule.h"
#include "control/performance/VelocityModule.h"

#include "routing/Routing.h"
#include "routing/RoutingBuffers.h"
#include "routing/RoutingProcess.h"

struct Voice {

    bool active = false;
    int  midiNote = -1;

    Oscillator oscA, oscB, oscC, sub;
    Noise noise;

    Filter filter0, filter1;

    OscParams   oscAParams, oscBParams, oscCParams, subParams;
    NoiseParams noiseParams;
    FilterParams filter0Params, filter1Params;

    Envelope env1;
    LFO      lfo1;

    AftertouchModule aftertouchMod;
    AlternateModule  alternateMod;
    ModWheelModule   modWheelMod;
    NoteModule       noteMod;
    PitchBendModule  pitchBendMod;
    RandomPerNoteModule  randomNoteMod;
    RandomPerVoiceModule randomVoiceMod;
    VelocityModule       velocityMod;

    float noteModValue = 0.0f;

    ModMatrix mod;

    std::array<SourceRouting, NUM_SOURCES> routing{};
    RoutingBuffers buffers;

    // ---------- routing helpers ----------
    void routeFilterBlend(int src, float blend)
    {
        routing[src].clear();
        routing[src].add(LayerTarget::FILTER0, 1.0f - blend);
        routing[src].add(LayerTarget::FILTER1, blend);
    }

    void routeMain(int src)   { routing[src].clear(); routing[src].add(LayerTarget::MAIN,   1.0f); }
    void routeDirect(int src) { routing[src].clear(); routing[src].add(LayerTarget::DIRECT, 1.0f); }
    void routeNone(int src)   { routing[src].clear(); }

    // ---------- start ----------
    void start(int note, float velocity, const Wavetable* wt)
    {
        active = true;
        midiNote = note;

        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        float f = oscA.midiNoteToFreq(note);

        auto initOsc = [&](OscParams& p)
        {
            p.freqBase = p.freqMod = f;
            p.semitoneBase = p.semitoneMod = 0.0f;
            p.fineBase = p.fineMod = 0.0f;
            p.levelBase = p.levelMod = 0.4f;
            p.panBase = p.panMod = 0.0f;
            p.wtPosBase = p.wtPosMod = 0.0f;
        };

        initOsc(oscAParams);
        initOsc(oscBParams);
        initOsc(oscCParams);
        initOsc(subParams);

        noiseParams.pitchBase = noiseParams.pitchMod = 0.0f;
        noiseParams.levelBase = noiseParams.levelMod = 0.3f;
        noiseParams.panBase   = noiseParams.panMod   = 0.0f;

        filter0Params = {};
        filter1Params = filter0Params;

        env1.noteOn(note, velocity);
        lfo1.retrigger();

        mod.clearAll();

        noteModValue = float(note - 60);
        noteMod.midiNote = &noteModValue;
        velocityMod.value = velocity;

        // -------- register sources --------
        mod.registerSource(ModSourceID::ENV1, &env1);
        mod.registerSource(ModSourceID::LFO1, &lfo1);
        mod.registerSource(ModSourceID::NOTE, &noteMod);
        mod.registerSource(ModSourceID::VELOCITY, &velocityMod);
        mod.registerSource(ModSourceID::MOD_WHEEL, &modWheelMod);
        mod.registerSource(ModSourceID::AFTERTOUCH, &aftertouchMod);
        mod.registerSource(ModSourceID::PITCH_BEND, &pitchBendMod);
        mod.registerSource(ModSourceID::ALTRNATE, &alternateMod);

        // ============================
        // REGISTER MOD TARGETS
        // ============================

        // -------- OSC A --------
        mod.registerTarget(ModTargetID::OSC_A_FREQ,      &oscAParams.freqMod,      oscAParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_A_SEMITONE,  &oscAParams.semitoneMod,  oscAParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_A_FINE,      &oscAParams.fineMod,      oscAParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_A_LEVEL,     &oscAParams.levelMod,     oscAParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_A_PAN,       &oscAParams.panMod,       oscAParams.panBase);
        mod.registerTarget(ModTargetID::OSC_A_WT_POS,    &oscAParams.wtPosMod,     oscAParams.wtPosBase);

        // -------- OSC B --------
        mod.registerTarget(ModTargetID::OSC_B_FREQ,      &oscBParams.freqMod,      oscBParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_B_SEMITONE,  &oscBParams.semitoneMod,  oscBParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_B_FINE,      &oscBParams.fineMod,      oscBParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_B_LEVEL,     &oscBParams.levelMod,     oscBParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_B_PAN,       &oscBParams.panMod,       oscBParams.panBase);
        mod.registerTarget(ModTargetID::OSC_B_WT_POS,    &oscBParams.wtPosMod,     oscBParams.wtPosBase);

        // -------- OSC C --------
        mod.registerTarget(ModTargetID::OSC_C_FREQ,      &oscCParams.freqMod,      oscCParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_C_SEMITONE,  &oscCParams.semitoneMod,  oscCParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_C_FINE,      &oscCParams.fineMod,      oscCParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_C_LEVEL,     &oscCParams.levelMod,     oscCParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_C_PAN,       &oscCParams.panMod,       oscCParams.panBase);
        mod.registerTarget(ModTargetID::OSC_C_WT_POS,    &oscCParams.wtPosMod,     oscCParams.wtPosBase);

        // -------- SUB --------
        mod.registerTarget(ModTargetID::SUB_FREQ,        &subParams.freqMod,       subParams.freqBase);
        mod.registerTarget(ModTargetID::SUB_SEMITONE,    &subParams.semitoneMod,   subParams.semitoneBase);
        mod.registerTarget(ModTargetID::SUB_FINE,        &subParams.fineMod,       subParams.fineBase);
        mod.registerTarget(ModTargetID::SUB_LEVEL,       &subParams.levelMod,      subParams.levelBase);
        mod.registerTarget(ModTargetID::SUB_PAN,         &subParams.panMod,        subParams.panBase);
        mod.registerTarget(ModTargetID::SUB_WT_POS,      &subParams.wtPosMod,      subParams.wtPosBase);

        // -------- NOISE --------
        mod.registerTarget(ModTargetID::NOISE_PITCH, &noiseParams.pitchMod, noiseParams.pitchBase);
        mod.registerTarget(ModTargetID::NOISE_LEVEL, &noiseParams.levelMod, noiseParams.levelBase);
        mod.registerTarget(ModTargetID::NOISE_PAN,   &noiseParams.panMod,   noiseParams.panBase);

        // -------- FILTER 0 --------
        mod.registerTarget(ModTargetID::FILTER0_CUTOFF, &filter0Params.cutoffMod, filter0Params.cutoffBase);
        mod.registerTarget(ModTargetID::FILTER0_RES,    &filter0Params.resMod,    filter0Params.resBase);
        mod.registerTarget(ModTargetID::FILTER0_DRIVE,  &filter0Params.driveMod,  filter0Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER0_FAT,    &filter0Params.driveMod,  filter0Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER0_MIX,    &filter0Params.mixMod,    filter0Params.mixBase);
        mod.registerTarget(ModTargetID::FILTER0_LEVEL,  &filter0Params.levelMod,  filter0Params.levelBase);
        mod.registerTarget(ModTargetID::FILTER0_PAN,    &filter0Params.panMod,    filter0Params.panBase);

        // -------- FILTER 1 --------
        mod.registerTarget(ModTargetID::FILTER1_CUTOFF, &filter1Params.cutoffMod, filter1Params.cutoffBase);
        mod.registerTarget(ModTargetID::FILTER1_RES,    &filter1Params.resMod,    filter1Params.resBase);
        mod.registerTarget(ModTargetID::FILTER1_DRIVE,  &filter1Params.driveMod,  filter1Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER1_FAT,    &filter1Params.driveMod,  filter1Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER1_MIX,    &filter1Params.mixMod,    filter1Params.mixBase);
        mod.registerTarget(ModTargetID::FILTER1_LEVEL,  &filter1Params.levelMod,  filter1Params.levelBase);
        mod.registerTarget(ModTargetID::FILTER1_PAN,    &filter1Params.panMod,    filter1Params.panBase);

        for (int i = 0; i < NUM_SOURCES; ++i)
            routing[i].clear();

        routeFilterBlend(0, 0.0f);
    }

    void stop() { env1.noteOff(); }

    // ---------- process ----------
    Sample process(float sr) {
        Sample out{};
        if (!active) return out;

        buffers.clear();

        // ---- ADVANCE CONTROL SOURCES ONCE ----
        env1.process(sr);
        lfo1.process(sr);
        noteMod.process(sr);
        velocityMod.process(sr);
        modWheelMod.process(sr);
        aftertouchMod.process(sr);
        pitchBendMod.process(sr);
        alternateMod.process(sr);
        randomVoiceMod.process(sr);
        randomNoteMod.process(sr);

        // ---- APPLY MODULATION (READ ONLY) ----
        mod.process(sr);

        // ---- AUDIO SOURCES ----
        VoiceSources src;
        src.oscA  = oscA.process(sr, oscAParams);
        src.oscB  = oscB.process(sr, oscBParams);
        src.oscC  = oscC.process(sr, oscCParams);
        src.sub   = sub.process(sr, subParams);
        src.noise = noise.process(sr, noiseParams);

        const Sample sources[NUM_SOURCES] = {
            src.oscA, src.oscB, src.oscC, src.sub, src.noise
        };

        for (int i = 0; i < NUM_SOURCES; ++i)
            routeSource(sources[i], routing[i], buffers);

        Sample f0 = filter0.process(sr, buffers.filterInput[0], filter0Params);
        Sample f1 = filter1.process(sr, buffers.filterInput[1], filter1Params);

        out.left  = buffers.main.left  + buffers.direct.left  + f0.left  + f1.left;
        out.right = buffers.main.right + buffers.direct.right + f0.right + f1.right;

        // ---- USE CACHED ENVELOPE VALUE ----
        float env = env1.getValue();
        out.left  *= env;
        out.right *= env;

        if (!env1.isActive())
            active = false;

        return out;
    }

};
