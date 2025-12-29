#pragma once

#include "engine/voice/VoiceState.h"
#include "engine/voice/VoiceRouting.h"
#include "engine/voice/VoiceDSP.h"
#include "engine/voice/VoiceInit.h"
#include "engine/voice/VoiceResolve.h"
#include "engine/voice/VoiceModulation.h"
#include "engine/voice/VoiceOutput.h"

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

    Voice() {
        // Default enabled sources
        routing.sources[0].enabled = true; // OSC A
        routing.filter[0].enabled  = true; // Filter 0
    }


    // ============================
    // STATE
    // ============================
    VoiceState state;

    // ============================
    // AUDIO
    // ============================
    Oscillator oscA, oscB, oscC, sub;
    Noise noise;
    Filter filter0, filter1;

    // ============================
    // PARAMS
    // ============================
    OscParams oscAParams, oscBParams, oscCParams, subParams;
    NoiseParams noiseParams;
    FilterParams filter0Params, filter1Params;

    // ============================
    // CONTROL
    // ============================
    Envelope env1;
    LFO lfo1;

    // ============================
    // PERFORMANCE (PER VOICE)
    // ============================
    NoteModule           noteMod;
    VelocityModule       velocityMod;
    ModWheelModule       modWheelMod;
    AftertouchModule     aftertouchMod;
    PitchBendModule      pitchBendMod;
    AlternateModule      alternateMod;
    RandomPerVoiceModule randomVoiceMod;
    RandomPerNoteModule  randomNoteMod;

    // ============================
    // MOD / ROUTING
    // ============================
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

    inline void registerOscTargets(
        OscParams& p,
        std::initializer_list<OscSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    inline void registerFilterTargets(
        FilterParams& p,
        std::initializer_list<FilterSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    inline void registerNoiseTargets(
        NoiseParams& p,
        std::initializer_list<NoiseSpec> specs)
    {
        for (auto& s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    // ============================
    // API
    // ============================

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
        VoiceInit::setSerumInitDefaults(
            routing,
            oscAParams,
            oscBParams,
            oscCParams,
            subParams,
            noiseParams
        );
    }

    void stop()
    {
        state.noteOff();
        env1.noteOff();
    }

    // ============================
    // AUDIO PROCESS
    // ============================

    VoiceOutput process(float sr)
    {
        state.samplesSinceNoteOn++;
        if (state.released)
            state.samplesSinceNoteOff++;

        if (!state.active)
            return {};

        // clear all routing buffers
        routing.buffers.clear();


        // small helper for debugging invalid / huge samples
        auto checkSampleVal = [](float v)->bool {
            if (!std::isfinite(v)) return false;
            // sanity clamp for debug; adjust threshold as needed
            return std::fabs(v) <= 1e3f;
        };

        auto debugCheck = [&](const Sample& s, const char* tag){
            if (!checkSampleVal(s.left) || !checkSampleVal(s.right)) {
                printf("[VOICE DEBUG] %s L=%f R=%f\n", tag, s.left, s.right);
            }
        };



        // ---- CONTROL ----
        float env = env1.process(sr);
        lfo1.process(sr);
        mod.process(sr);

        // ---- SOURCES ----
        for (int i = 0; i < NUM_SOURCES; ++i) {

            if (!routing.sources[i].enabled)
                continue;

            Sample s{};
            switch (i) {
                case 0: s = oscA.process(sr, VoiceResolve::resolveOsc(oscAParams, sr)); break;
                case 1: s = oscB.process(sr, VoiceResolve::resolveOsc(oscBParams, sr)); break;
                case 2: s = oscC.process(sr, VoiceResolve::resolveOsc(oscCParams, sr)); break;
                case 3: s = sub.process (sr, VoiceResolve::resolveOsc(subParams,  sr)); break;
                case 4: s = noise.process(sr, VoiceResolve::resolveNoise(noiseParams)); break;
            }

            // amp envelope BEFORE routing
            s.left  *= env;
            s.right *= env;

            routeSource(s, routing.routing[i], routing.buffers);
        }

        // ---- FILTER 0 ----
        Sample f0{};
        if (routing.filter[0].enabled) {
            f0 = VoiceDSP::processFilter(
                filter0,
                filter0Params,
                0,
                routing.filter[0].route,
                routing.buffers,
                sr
            );
        } else {
            // FILTER BYPASS: passthrough input
            f0 = routing.buffers.filterInput[0];
            routing.buffers.filterInput[0] = {};
        }


        routing.buffers.bus1.left  += f0.left  * routing.filter[0].bus1Send;
        routing.buffers.bus1.right += f0.right * routing.filter[0].bus1Send;

        routing.buffers.bus2.left  += f0.left  * routing.filter[0].bus2Send;
        routing.buffers.bus2.right += f0.right * routing.filter[0].bus2Send;

        // ---- FILTER 1 ----
        Sample f1{};
        if (routing.filter[1].enabled) {
            f1 = VoiceDSP::processFilter(
                filter1,
                filter1Params,
                1,
                routing.filter[1].route,
                routing.buffers,
                sr
            );
        } else {
            // FILTER BYPASS: passthrough input
            f1 = routing.buffers.filterInput[1];
            routing.buffers.filterInput[1] = {};
        }


        routing.buffers.bus1.left  += f1.left  * routing.filter[1].bus1Send;
        routing.buffers.bus1.right += f1.right * routing.filter[1].bus1Send;

        routing.buffers.bus2.left  += f1.left  * routing.filter[1].bus2Send;
        routing.buffers.bus2.right += f1.right * routing.filter[1].bus2Send;

        // ---- BUS PROCESSING ----
        VoiceDSP::processBus(
            routing.buffers.bus1,
            0,
            routing.busRoute[0],
            routing.buffers
        );

        VoiceDSP::processBus(
            routing.buffers.bus2,
            1,
            routing.busRoute[1],
            routing.buffers
        );
        
        // ---- OUTPUT ----
        VoiceOutput out{};
        out.main   = routing.buffers.main;
        out.bus1   = routing.buffers.bus1;
        out.bus2   = routing.buffers.bus2;
        out.direct = routing.buffers.direct;

        if (!env1.isActive())
            state.active = false;

        return out;

    }
};
