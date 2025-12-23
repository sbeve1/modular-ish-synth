#pragma once

#include "engine/voice/VoiceState.h"
#include "engine/voice/VoiceRouting.h"
#include "engine/voice/VoiceDSP.h"
#include "engine/voice/VoiceInit.h"
#include "engine/voice/VoiceResolve.h"
#include "engine/voice/VoiceModulation.h"

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Filter.h"
#include "control/Envelope.h"
#include "control/LFO.h"
#include "engine/ModMatrix.h"

#include "control/performance/NoteModule.h"
#include "control/performance/VelocityModule.h"
#include "control/performance/ModWheelModule.h"
#include "control/performance/AftertouchModule.h"
#include "control/performance/PitchBendModule.h"
#include "control/performance/AlternateModule.h"
#include "control/performance/RandomPerVoiceModule.h"
#include "control/performance/RandomPerNoteModule.h"

struct Voice {

    // -------- STATE --------
    VoiceState state;

    // -------- AUDIO --------
    Oscillator oscA, oscB, oscC, sub;
    Noise noise;
    Filter filter0, filter1;

    // -------- PARAMS --------
    OscParams oscAParams, oscBParams, oscCParams, subParams;
    NoiseParams noiseParams;
    FilterParams filter0Params, filter1Params;

    // -------- CONTROL --------
    Envelope env1;
    LFO lfo1;

    // -------- PERFORMANCE (PER VOICE) --------
    NoteModule           noteMod;
    VelocityModule       velocityMod;
    ModWheelModule       modWheelMod;
    AftertouchModule     aftertouchMod;
    PitchBendModule      pitchBendMod;
    AlternateModule      alternateMod;
    RandomPerVoiceModule randomVoiceMod;
    RandomPerNoteModule  randomNoteMod;

    // -------- MOD / ROUTING --------
    ModMatrix mod;
    VoiceRouting routing;

    // ============================
    // MOD TARGET REGISTRATION HELPERS
    // ============================

    struct OscSpec {
        ModTargetID id;
        float OscParams::* modPtr;
        float OscParams::* basePtr;
    };

    struct FilterSpec {
        ModTargetID id;
        float FilterParams::* modPtr;
        float FilterParams::* basePtr;
    };

    struct NoiseSpec {
        ModTargetID id;
        float NoiseParams::* modPtr;
        float NoiseParams::* basePtr;
    };

    inline void registerOscTargets(OscParams& p,
                                std::initializer_list<OscSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    inline void registerFilterTargets(FilterParams& p,
                                    std::initializer_list<FilterSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    inline void registerNoiseTargets(NoiseParams& p,
                                    std::initializer_list<NoiseSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }


    // -------- API --------
    void start(int note, float velocity, const Wavetable* wt)
    {
        state.noteOn(note);

        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        oscAParams.midiNote = note;
        oscBParams.midiNote = note;
        oscCParams.midiNote = note;
        subParams.midiNote  = note;

        env1.noteOn(note, velocity);
        lfo1.retrigger();

        VoiceModulation::registerSources(*this);
        VoiceModulation::registerTargets(*this);
        VoiceInit::setSerumInitDefaults(routing);
    }

    void stop()
    {
        state.noteOff();
        env1.noteOff();
    }


    Sample process(float sr)
    {

        state.samplesSinceNoteOn++;
        if (state.released)
            state.samplesSinceNoteOff++;

        if (!state.active)
            return {};

        routing.buffers.clear();

        // ---- CONTROL ----
        float env = env1.process(sr);
        lfo1.process(sr);
        mod.process(sr);

        // ---- AUDIO SOURCES ----
        for (int i = 0; i < NUM_SOURCES; ++i) {

            Sample s{};

            switch (i) {
                case 0: // OSC A
                    s = oscA.process(sr, VoiceResolve::resolveOsc(oscAParams, sr));
                    break;

                case 1: // OSC B
                    s = oscB.process(sr, VoiceResolve::resolveOsc(oscBParams, sr));
                    break;

                case 2: // OSC C
                    s = oscC.process(sr, VoiceResolve::resolveOsc(oscCParams, sr));
                    break;

                case 3: // SUB
                    s = sub.process(sr, VoiceResolve::resolveOsc(subParams, sr));
                    break;

                case 4: // NOISE
                    s = noise.process(sr, noiseParams);
                    break;
            }

            // Serum-style amp envelope BEFORE routing
            s.left  *= env;
            s.right *= env;

            // Route according to graph (or to nowhere)
            routeSource(s, routing.routing[i], routing.buffers);
        }

        // ---- FILTERS ----
        VoiceDSP::processFilter(
            filter0, filter0Params,
            0, routing.filter0Route,
            routing.buffers, sr
        );

        VoiceDSP::processFilter(
            filter1, filter1Params,
            1, routing.filter1Route,
            routing.buffers, sr
        );

        Sample out;
        out.left  = routing.buffers.fx.left  + routing.buffers.direct.left;
        out.right = routing.buffers.fx.right + routing.buffers.direct.right;

        if (!env1.isActive())
            state.active = false;

        return out;
    }
};

