#pragma once

#include <array>
#include <cstdlib>
#include <cstdint>

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

    // ============================
    // AUDIO MODULES
    // ============================
    Oscillator oscA, oscB, oscC, sub;
    Noise noise;

    Filter filter0, filter1;

    // ============================
    // PARAM BLOCKS
    // ============================
    OscParams    oscAParams, oscBParams, oscCParams, subParams;
    NoiseParams  noiseParams;
    FilterParams filter0Params, filter1Params;

    // ============================
    // CONTROL SOURCES
    // ============================
    Envelope env1;
    LFO      lfo1;

    AftertouchModule     aftertouchMod;
    AlternateModule      alternateMod;
    ModWheelModule       modWheelMod;
    NoteModule           noteMod;
    PitchBendModule      pitchBendMod;
    RandomPerNoteModule  randomNoteMod;
    RandomPerVoiceModule randomVoiceMod;
    VelocityModule       velocityMod;

    // ============================
    // MODULATION
    // ============================
    ModMatrix mod;

    // ============================
    // ROUTING
    // ============================
    std::array<SourceRouting, NUM_SOURCES> routing{};
    RoutingBuffers buffers;

    // ============================
    // ROUTING HELPERS
    // ============================
    void routeMain(int src)
    {
        routing[src].clear();
        routing[src].add(LayerTarget::MAIN, 1.0f);
    }

    void routeFilterBlend(int src, float blend)
    {
        routing[src].clear();
        routing[src].add(LayerTarget::FILTER0, 1.0f - blend);
        routing[src].add(LayerTarget::FILTER1, blend);
    }

    // ============================
    // NOTE START
    // ============================
    void start(int note, float velocity, const Wavetable* wt)
    {
        active   = true;
        midiNote = note;
        
        // ---------- reset filter state (prevent runaway bleed) ----------
        filter0.ic1eqL = filter0.ic2eqL = 0.0f;
        filter0.ic1eqR = filter0.ic2eqR = 0.0f;
        filter1.ic1eqL = filter1.ic2eqL = 0.0f;
        filter1.ic1eqR = filter1.ic2eqR = 0.0f;


        // ---------- wavetable ----------
        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        float f = oscA.midiNoteToFreq(note);

        // ---------- oscillator defaults ----------
        auto initOsc = [&](OscParams& p)
        {
            p.freqBase     = p.freqMod     = f;
            p.semitoneBase = p.semitoneMod = 0.0f;
            p.fineBase     = p.fineMod     = 0.0f;

            p.levelBase    = 0.25f;
            p.levelMod     = 0.0f;
            p.panBase      = 0.0f;
            p.panMod       = 0.0f;
            p.wtPosBase    = 0.0f;
            p.wtPosMod     = 0.0f;

            p.unisonVoices = 1.0f;
            p.detune       = 0.0f;
            p.stereoSpread = 0.0f;
            p.phaseSpread  = 0.0f;
            p.randomPhase  = true;
        };

        initOsc(oscAParams);
        initOsc(oscBParams);
        initOsc(oscCParams);
        initOsc(subParams);

        oscA.resetPhase(true);
        oscB.resetPhase(true);
        oscC.resetPhase(true);
        sub.resetPhase(false);

        // ---------- noise ----------
        noiseParams.pitchBase = 0.0f;
        noiseParams.pitchMod  = 0.0f;
        noiseParams.levelBase = 0.15f;
        noiseParams.levelMod  = 0.0f;
        noiseParams.panBase   = 0.0f;
        noiseParams.panMod    = 0.0f;

        noise.reset((uint32_t)(note * 2654435761u) ^ (uint32_t)std::rand());

        // ---------- filters (IMPORTANT: not zero) ----------
        filter0Params = {};
        filter0Params.cutoffBase = 1000.0f;
        filter0Params.mixBase    = 1.0f;
        filter0Params.levelBase  = 1.0f;

        filter1Params = filter0Params;

        // ---------- envelope / lfo ----------
        env1.noteOn(note, velocity);
        lfo1.retrigger();

        // ---------- modulation ----------
        mod.clearAll();

        noteMod.value     = float(note - 60);
        velocityMod.value = velocity;

        // ============================
        // REGISTER SOURCES
        // ============================
        mod.registerSource(ModSourceID::ENV1,       &env1);
        mod.registerSource(ModSourceID::LFO1,       &lfo1);
        mod.registerSource(ModSourceID::NOTE,       &noteMod);
        mod.registerSource(ModSourceID::VELOCITY,   &velocityMod);
        mod.registerSource(ModSourceID::MOD_WHEEL,  &modWheelMod);
        mod.registerSource(ModSourceID::AFTERTOUCH, &aftertouchMod);
        mod.registerSource(ModSourceID::PITCH_BEND, &pitchBendMod);
        mod.registerSource(ModSourceID::ALTRNATE,   &alternateMod);

        // ============================
        // REGISTER TARGETS — FILTER 0
        // ============================
        mod.registerTarget(ModTargetID::FILTER0_CUTOFF, &filter0Params.cutoffMod, filter0Params.cutoffBase);
        mod.registerTarget(ModTargetID::FILTER0_RES,    &filter0Params.resMod,    filter0Params.resBase);
        mod.registerTarget(ModTargetID::FILTER0_DRIVE,  &filter0Params.driveMod,  filter0Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER0_FAT,    &filter0Params.fatMod,    filter0Params.fatBase);
        mod.registerTarget(ModTargetID::FILTER0_MIX,    &filter0Params.mixMod,    filter0Params.mixBase);
        mod.registerTarget(ModTargetID::FILTER0_PAN,    &filter0Params.panMod,    filter0Params.panBase);
        mod.registerTarget(ModTargetID::FILTER0_LEVEL,  &filter0Params.levelMod,  filter0Params.levelBase);

        // ============================
        // REGISTER TARGETS — FILTER 1
        // ============================
        mod.registerTarget(ModTargetID::FILTER1_CUTOFF, &filter1Params.cutoffMod, filter1Params.cutoffBase);
        mod.registerTarget(ModTargetID::FILTER1_RES,    &filter1Params.resMod,    filter1Params.resBase);
        mod.registerTarget(ModTargetID::FILTER1_DRIVE,  &filter1Params.driveMod,  filter1Params.driveBase);
        mod.registerTarget(ModTargetID::FILTER1_FAT,    &filter1Params.fatMod,    filter1Params.fatBase);
        mod.registerTarget(ModTargetID::FILTER1_MIX,    &filter1Params.mixMod,    filter1Params.mixBase);
        mod.registerTarget(ModTargetID::FILTER1_PAN,    &filter1Params.panMod,    filter1Params.panBase);
        mod.registerTarget(ModTargetID::FILTER1_LEVEL,  &filter1Params.levelMod,  filter1Params.levelBase);

        // ============================
        // REGISTER TARGETS — OSC A
        // ============================
        mod.registerTarget(ModTargetID::OSC_A_FREQ,     &oscAParams.freqMod,     oscAParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_A_SEMITONE, &oscAParams.semitoneMod, oscAParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_A_FINE,     &oscAParams.fineMod,     oscAParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_A_LEVEL,    &oscAParams.levelMod,    oscAParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_A_PAN,      &oscAParams.panMod,      oscAParams.panBase);
        mod.registerTarget(ModTargetID::OSC_A_WT_POS,   &oscAParams.wtPosMod,    oscAParams.wtPosBase);

        mod.registerTarget(ModTargetID::OSC_A_UNISON, &oscAParams.unisonVoices, oscAParams.unisonVoices);
        mod.registerTarget(ModTargetID::OSC_A_DETUNE, &oscAParams.detune,       oscAParams.detune);
        mod.registerTarget(ModTargetID::OSC_A_STEREO, &oscAParams.stereoSpread, oscAParams.stereoSpread);
        mod.registerTarget(ModTargetID::OSC_A_PHASE,  &oscAParams.phaseSpread,  oscAParams.phaseSpread);

        // ============================
        // REGISTER TARGETS — OSC B
        // ============================
        mod.registerTarget(ModTargetID::OSC_B_FREQ,     &oscBParams.freqMod,     oscBParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_B_SEMITONE, &oscBParams.semitoneMod, oscBParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_B_FINE,     &oscBParams.fineMod,     oscBParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_B_LEVEL,    &oscBParams.levelMod,    oscBParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_B_PAN,      &oscBParams.panMod,      oscBParams.panBase);
        mod.registerTarget(ModTargetID::OSC_B_WT_POS,   &oscBParams.wtPosMod,    oscBParams.wtPosBase);

        mod.registerTarget(ModTargetID::OSC_B_UNISON, &oscBParams.unisonVoices, oscBParams.unisonVoices);
        mod.registerTarget(ModTargetID::OSC_B_DETUNE, &oscBParams.detune,       oscBParams.detune);
        mod.registerTarget(ModTargetID::OSC_B_STEREO, &oscBParams.stereoSpread, oscBParams.stereoSpread);
        mod.registerTarget(ModTargetID::OSC_B_PHASE,  &oscBParams.phaseSpread,  oscBParams.phaseSpread);

        // ============================
        // REGISTER TARGETS — OSC C
        // ============================
        mod.registerTarget(ModTargetID::OSC_C_FREQ,     &oscCParams.freqMod,     oscCParams.freqBase);
        mod.registerTarget(ModTargetID::OSC_C_SEMITONE, &oscCParams.semitoneMod, oscCParams.semitoneBase);
        mod.registerTarget(ModTargetID::OSC_C_FINE,     &oscCParams.fineMod,     oscCParams.fineBase);
        mod.registerTarget(ModTargetID::OSC_C_LEVEL,    &oscCParams.levelMod,    oscCParams.levelBase);
        mod.registerTarget(ModTargetID::OSC_C_PAN,      &oscCParams.panMod,      oscCParams.panBase);
        mod.registerTarget(ModTargetID::OSC_C_WT_POS,   &oscCParams.wtPosMod,    oscCParams.wtPosBase);

        mod.registerTarget(ModTargetID::OSC_C_UNISON, &oscCParams.unisonVoices, oscCParams.unisonVoices);
        mod.registerTarget(ModTargetID::OSC_C_DETUNE, &oscCParams.detune,       oscCParams.detune);
        mod.registerTarget(ModTargetID::OSC_C_STEREO, &oscCParams.stereoSpread, oscCParams.stereoSpread);
        mod.registerTarget(ModTargetID::OSC_C_PHASE,  &oscCParams.phaseSpread,  oscCParams.phaseSpread);

        // ============================
        // REGISTER TARGETS — SUB
        // ============================
        mod.registerTarget(ModTargetID::SUB_FREQ,     &subParams.freqMod,     subParams.freqBase);
        mod.registerTarget(ModTargetID::SUB_SEMITONE, &subParams.semitoneMod, subParams.semitoneBase);
        mod.registerTarget(ModTargetID::SUB_FINE,     &subParams.fineMod,     subParams.fineBase);
        mod.registerTarget(ModTargetID::SUB_LEVEL,    &subParams.levelMod,    subParams.levelBase);
        mod.registerTarget(ModTargetID::SUB_PAN,      &subParams.panMod,      subParams.panBase);
        mod.registerTarget(ModTargetID::SUB_WT_POS,   &subParams.wtPosMod,    subParams.wtPosBase);

        // ============================
        // REGISTER TARGETS — NOISE
        // ============================
        mod.registerTarget(ModTargetID::NOISE_PITCH, &noiseParams.pitchMod, noiseParams.pitchBase);
        mod.registerTarget(ModTargetID::NOISE_LEVEL, &noiseParams.levelMod, noiseParams.levelBase);
        mod.registerTarget(ModTargetID::NOISE_PAN,   &noiseParams.panMod,   noiseParams.panBase);

        // ============================
        // ROUTING (CRITICAL)
        // ============================
        for (int i = 0; i < NUM_SOURCES; ++i)
            routing[i].clear();

        // Default audible path
        routeMain(0);
    }

    void stop()
    {
        env1.noteOff();
    }

    Sample process(float sr)
    {
        Sample out{};
        if (!active) return out;

        buffers.clear();

        // ---- CONTROL ----
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

        mod.process(sr);

        // ---- AUDIO ----
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

        // ---- VCA ----
        float env = env1.getValue();
        out.left  *= env;
        out.right *= env;

        if (!env1.isActive())
            active = false;

        return out;
    }
};
