#pragma once

#include <array>
#include <cstdlib>
#include <cstdint>
#include <initializer_list>

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Wavetable.h"
#include "audio/Filter.h"

#include "engine/ModMatrix.h"
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

#include "routing/Routing.h"       // provides: NUM_SOURCES, SourceRouting, routeSource(...)
#include "engine/VoiceGraph.h"    // provides: SourceRouteConfig, FilterRoute, SourceDest

struct Voice {

    bool active = false;
    int  midiNote = -1;
    int noteId = -1;
    uint32_t samplesSinceNoteOn = 0;
    uint32_t samplesSinceNoteOff = 0;
    bool released = false;

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
    // Per-source routing table (bounded routes per source)
    std::array<SourceRouting, NUM_SOURCES> routing{};
    // Per-voice routing buffers (filter inputs, FX, Direct)
    VoiceRoutingBuffers buffers;

    // High-level graph config (useful for UI/preset layer)
    std::array<SourceRouteConfig, NUM_SOURCES> sourceRoutes{};
    FilterRoute filter0Route = FilterRoute::TO_FX;
    FilterRoute filter1Route = FilterRoute::TO_FX;


    // ============================
    // Small helper types for mod registration
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

    // ============================
    // ROUTING HELPERS
    // ============================
    inline void configureSourceRoutingFromGraph(int src)
    {
        routing[src].clear();
        const auto& cfg = sourceRoutes[src];

        switch (cfg.dest) {

            case SourceDest::NONE:
                // no routes → signal ends here
                break;

            case SourceDest::DIRECT:
                routing[src].add(LayerTarget::DIRECT, 1.0f);
                break;

            case SourceDest::FX:
                routing[src].add(LayerTarget::FX, 1.0f);
                break;

            case SourceDest::FILTER:
                routing[src].add(LayerTarget::FILTER0, 1.0f - cfg.filterBlend);
                routing[src].add(LayerTarget::FILTER1, cfg.filterBlend);
                break;
        }
    }


    // Enforce graph rules and resolve routing tables
    void applyRoutingGraph()
    {
        for (int i = 0; i < NUM_SOURCES; ++i)
            configureSourceRoutingFromGraph(i);

        // prevent filter feedback loop
        if (filter0Route == FilterRoute::TO_OTHER_FILTER &&
            filter1Route == FilterRoute::TO_OTHER_FILTER)
        {
            filter1Route = FilterRoute::TO_FX;
        }
    }

    // ============================
    // ROUTING API — SOURCE INTENT
    // ============================

    // Turn source completely OFF (signal terminates)
    inline void setSourceOff(int src)
    {
        sourceRoutes[src].dest = SourceDest::NONE;
        sourceRoutes[src].filterBlend = 0.0f;
    }

    // Route source to FILTER chain with blend
    // blend = 0.0 → 100% Filter0
    // blend = 1.0 → 100% Filter1
    inline void setSourceToFilter(int src, float blend)
    {
        sourceRoutes[src].dest = SourceDest::FILTER;
        sourceRoutes[src].filterBlend = std::clamp(blend, 0.0f, 1.0f);
    }

    // Route source directly to FX (main path)
    inline void setSourceToFX(int src)
    {
        sourceRoutes[src].dest = SourceDest::FX;
    }

    // Route source directly to DIRECT (dry bypass)
    inline void setSourceToDirect(int src)
    {
        sourceRoutes[src].dest = SourceDest::DIRECT;
    }


    // ============================
    // ROUTING API — FILTER INTENT
    // ============================

    // Turn filter OFF (signal terminates)
    inline void setFilterOff(int idx)
    {
        if (idx == 0)
            filter0Route = FilterRoute::NONE;
        else
            filter1Route = FilterRoute::NONE;
    }

    // Route filter output to FX (main path)
    inline void setFilterToFX(int idx)
    {
        if (idx == 0)
            filter0Route = FilterRoute::TO_FX;
        else
            filter1Route = FilterRoute::TO_FX;
    }

    // Route filter output to DIRECT (dry bypass)
    inline void setFilterToDirect(int idx)
    {
        if (idx == 0)
            filter0Route = FilterRoute::TO_DIRECT;
        else
            filter1Route = FilterRoute::TO_DIRECT;
    }

    // Chain filter into the other filter
    inline void chainFilterToOther(int idx)
    {
        if (idx == 0)
            filter0Route = FilterRoute::TO_OTHER_FILTER;
        else
            filter1Route = FilterRoute::TO_OTHER_FILTER;
    }

    // ============================
    // SERUM INIT PATCH (SINGLE SOURCE OF TRUTH)
    // ============================
    void setSerumInitDefaults()
    {
        // All sources OFF by default
        for (int i = 0; i < NUM_SOURCES; ++i) {
            sourceRoutes[i].dest = SourceDest::NONE;
            sourceRoutes[i].filterBlend = 0.0f;
        }

        // OSC A → FILTER (100% Filter 0)
        sourceRoutes[0].dest = SourceDest::FILTER;
        sourceRoutes[0].filterBlend = 0.0f;

        // Filters → FX
        filter0Route = FilterRoute::TO_FX;
        filter1Route = FilterRoute::TO_FX;

        applyRoutingGraph();
    }



    // ============================
    // Helper: register osc targets via a small table (avoids repeated calls)
    // Call like: registerOscTargets(oscAParams, { {ModTargetID::OSC_A_SEMITONE, &OscParams::semitoneMod, &OscParams::semitoneBase}, ... });
    // ============================
    void registerOscTargets(OscParams& p, std::initializer_list<OscSpec> specs)
    {
        for (auto &s : specs) {
            // address of the mod field in this params instance
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    void registerFilterTargets(FilterParams& p, std::initializer_list<FilterSpec> specs)
    {
        for (auto &s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    void registerNoiseTargets(NoiseParams& p, std::initializer_list<NoiseSpec> specs)
    {
        for (auto &s : specs) {
            float* modAddr = &(p.*(s.modPtr));
            float  baseVal = p.*(s.basePtr);
            mod.registerTarget(s.id, modAddr, baseVal);
        }
    }

    // ============================
    // Small helper to process filter + routing (shared by both filters)
    // index must be 0 or 1
    // ============================
    inline void processFilter(
        Filter& filter,
        FilterParams& params,
        int index,
        FilterRoute route,
        VoiceRoutingBuffers& buffers,
        float sr)
    {
        if (route == FilterRoute::NONE)
            return; // terminate signal

        auto& in = buffers.filterInput[index];

        Sample out = filter.process(sr, in, params);

        switch (route) {
            case FilterRoute::TO_FX:
                buffers.fx.left  += out.left;
                buffers.fx.right += out.right;
                break;

            case FilterRoute::TO_DIRECT:
                buffers.direct.left  += out.left;
                buffers.direct.right += out.right;
                break;

            case FilterRoute::TO_OTHER_FILTER:
                buffers.filterInput[1 - index].left  += out.left;
                buffers.filterInput[1 - index].right += out.right;
                break;

            case FilterRoute::NONE:
                // already handled
                break;
        }
    }



    // ============================
    // Helper to generate individual source by index (explicit switch, no generalization)
    // indices: 0=oscA,1=oscB,2=oscC,3=sub,4=noise
    // ============================
    inline Sample processSource(int idx, float sr)
    {
        switch (idx) {
            case 0: return oscA.process(sr, oscAParams);
            case 1: return oscB.process(sr, oscBParams);
            case 2: return oscC.process(sr, oscCParams);
            case 3: return sub.process(sr, subParams);
            case 4: return noise.process(sr, noiseParams);
            default: return Sample{};
        }
    }

    // ============================
    // NOTE START
    // ============================
    void start(int note, float velocity, const Wavetable* wt)
    {
        active   = true;
        midiNote = note;
        released = false;
        samplesSinceNoteOn = 0;
        samplesSinceNoteOff = 0;

        // ---------- wavetable ----------
        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        // ---------- oscillator defaults ----------
        auto initOsc = [&](OscParams& p)
        {
            p.semitoneBase = 0.0f;
            p.fineBase     = 0.0f;

            p.levelBase    = DB_MINUS_10; // −10 dB
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

        oscAParams.midiNote = note;
        oscBParams.midiNote = note;
        oscCParams.midiNote = note;
        subParams.midiNote  = note;

        // ---------- noise ----------
        noiseParams.pitchBase = 0.0f;
        noiseParams.pitchMod  = 0.0f;
        noiseParams.levelBase = DB_MINUS_20; // −20 dB
        noiseParams.levelMod  = 0.0f;
        noiseParams.panBase   = 0.0f;
        noiseParams.panMod    = 0.0f;

        noise.reset((uint32_t)(note * 2654435761u) ^ (uint32_t)std::rand());

        // ---------- filters ----------
        filter0Params = {};
        filter0Params.cutoffBase = 1000.0f;
        filter0Params.mixBase    = 1.0f;
        filter0Params.levelBase  = 1.0f;

        filter1Params = filter0Params;

        // ---------- envelope / lfo ----------
        env1.noteOn(note, velocity);
        lfo1.retrigger();

        // ---------- modulation (clear + register) ----------
        mod.clearAll();

        // register sources
        mod.registerSource(ModSourceID::ENV1,       &env1);
        mod.registerSource(ModSourceID::LFO1,       &lfo1);
        mod.registerSource(ModSourceID::NOTE,       &noteMod);
        mod.registerSource(ModSourceID::VELOCITY,   &velocityMod);
        mod.registerSource(ModSourceID::MOD_WHEEL,  &modWheelMod);
        mod.registerSource(ModSourceID::AFTERTOUCH, &aftertouchMod);
        mod.registerSource(ModSourceID::PITCH_BEND, &pitchBendMod);
        mod.registerSource(ModSourceID::ALTRNATE,   &alternateMod);
        mod.registerSource(ModSourceID::RANDOM_PER_VOICE, &randomVoiceMod);
        mod.registerSource(ModSourceID::RANDOM_PER_NOTE,  &randomNoteMod);

        // register targets — use helper tables to avoid repetitive lines
        registerFilterTargets(filter0Params, {
            { ModTargetID::FILTER0_CUTOFF, &FilterParams::cutoffMod, &FilterParams::cutoffBase },
            { ModTargetID::FILTER0_RES,    &FilterParams::resMod,    &FilterParams::resBase },
            { ModTargetID::FILTER0_DRIVE,  &FilterParams::driveMod,  &FilterParams::driveBase },
            { ModTargetID::FILTER0_FAT,    &FilterParams::fatMod,    &FilterParams::fatBase },
            { ModTargetID::FILTER0_MIX,    &FilterParams::mixMod,    &FilterParams::mixBase },
            { ModTargetID::FILTER0_PAN,    &FilterParams::panMod,    &FilterParams::panBase },
            { ModTargetID::FILTER0_LEVEL,  &FilterParams::levelMod,  &FilterParams::levelBase }
        });

        registerFilterTargets(filter1Params, {
            { ModTargetID::FILTER1_CUTOFF, &FilterParams::cutoffMod, &FilterParams::cutoffBase },
            { ModTargetID::FILTER1_RES,    &FilterParams::resMod,    &FilterParams::resBase },
            { ModTargetID::FILTER1_DRIVE,  &FilterParams::driveMod,  &FilterParams::driveBase },
            { ModTargetID::FILTER1_FAT,    &FilterParams::fatMod,    &FilterParams::fatBase },
            { ModTargetID::FILTER1_MIX,    &FilterParams::mixMod,    &FilterParams::mixBase },
            { ModTargetID::FILTER1_PAN,    &FilterParams::panMod,    &FilterParams::panBase },
            { ModTargetID::FILTER1_LEVEL,  &FilterParams::levelMod,  &FilterParams::levelBase }
        });

        registerOscTargets(oscAParams, {
            { ModTargetID::OSC_A_SEMITONE, &OscParams::semitoneMod, &OscParams::semitoneBase },
            { ModTargetID::OSC_A_FINE,     &OscParams::fineMod,     &OscParams::fineBase },
            { ModTargetID::OSC_A_LEVEL,    &OscParams::levelMod,    &OscParams::levelBase },
            { ModTargetID::OSC_A_PAN,      &OscParams::panMod,      &OscParams::panBase },
            { ModTargetID::OSC_A_WT_POS,   &OscParams::wtPosMod,    &OscParams::wtPosBase },
            { ModTargetID::OSC_A_UNISON,   &OscParams::unisonVoices,&OscParams::unisonVoices },
            { ModTargetID::OSC_A_DETUNE,   &OscParams::detune,      &OscParams::detune },
            { ModTargetID::OSC_A_STEREO,   &OscParams::stereoSpread,&OscParams::stereoSpread },
            { ModTargetID::OSC_A_PHASE,    &OscParams::phaseSpread, &OscParams::phaseSpread }
        });

        registerOscTargets(oscBParams, {
            { ModTargetID::OSC_B_SEMITONE, &OscParams::semitoneMod, &OscParams::semitoneBase },
            { ModTargetID::OSC_B_FINE,     &OscParams::fineMod,     &OscParams::fineBase },
            { ModTargetID::OSC_B_LEVEL,    &OscParams::levelMod,    &OscParams::levelBase },
            { ModTargetID::OSC_B_PAN,      &OscParams::panMod,      &OscParams::panBase },
            { ModTargetID::OSC_B_WT_POS,   &OscParams::wtPosMod,    &OscParams::wtPosBase },
            { ModTargetID::OSC_B_UNISON,   &OscParams::unisonVoices,&OscParams::unisonVoices },
            { ModTargetID::OSC_B_DETUNE,   &OscParams::detune,      &OscParams::detune },
            { ModTargetID::OSC_B_STEREO,   &OscParams::stereoSpread,&OscParams::stereoSpread },
            { ModTargetID::OSC_B_PHASE,    &OscParams::phaseSpread, &OscParams::phaseSpread }
        });

        registerOscTargets(oscCParams, {
            { ModTargetID::OSC_C_SEMITONE, &OscParams::semitoneMod, &OscParams::semitoneBase },
            { ModTargetID::OSC_C_FINE,     &OscParams::fineMod,     &OscParams::fineBase },
            { ModTargetID::OSC_C_LEVEL,    &OscParams::levelMod,    &OscParams::levelBase },
            { ModTargetID::OSC_C_PAN,      &OscParams::panMod,      &OscParams::panBase },
            { ModTargetID::OSC_C_WT_POS,   &OscParams::wtPosMod,    &OscParams::wtPosBase },
            { ModTargetID::OSC_C_UNISON,   &OscParams::unisonVoices,&OscParams::unisonVoices },
            { ModTargetID::OSC_C_DETUNE,   &OscParams::detune,      &OscParams::detune },
            { ModTargetID::OSC_C_STEREO,   &OscParams::stereoSpread,&OscParams::stereoSpread },
            { ModTargetID::OSC_C_PHASE,    &OscParams::phaseSpread, &OscParams::phaseSpread }
        });

        registerOscTargets(subParams, {
            { ModTargetID::SUB_SEMITONE, &OscParams::semitoneMod, &OscParams::semitoneBase },
            { ModTargetID::SUB_FINE,     &OscParams::fineMod,     &OscParams::fineBase },
            { ModTargetID::SUB_LEVEL,    &OscParams::levelMod,    &OscParams::levelBase },
            { ModTargetID::SUB_PAN,      &OscParams::panMod,      &OscParams::panBase },
            { ModTargetID::SUB_WT_POS,   &OscParams::wtPosMod,    &OscParams::wtPosBase }
        });

        registerNoiseTargets(noiseParams, {
            { ModTargetID::NOISE_PITCH, &NoiseParams::pitchMod, &NoiseParams::pitchBase },
            { ModTargetID::NOISE_LEVEL, &NoiseParams::levelMod, &NoiseParams::levelBase },
            { ModTargetID::NOISE_PAN,   &NoiseParams::panMod,   &NoiseParams::panBase }
        });

        setSerumInitDefaults();
    }

    void stop()
    {
        released = true;
        samplesSinceNoteOff = 0;
        env1.noteOff();
    }

    // ============================
    // PROCESS
    // ============================
    Sample process(float sr)
    {
        samplesSinceNoteOn++;

        if (released)
            samplesSinceNoteOff++;

        Sample out{};
        if (!active) return out;

        buffers.clear();

        // ---- CONTROL ----
        float env = env1.process(sr);

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
        // generate each source and immediately apply amp envelope and route
        for (int i = 0; i < NUM_SOURCES; ++i) {
            Sample s = processSource(i, sr);

            // APPLY AMP ENVELOPE PRE-ROUTING (SERUM STYLE)
            s.left  *= env;
            s.right *= env;

            // route sources into voice buffers
            routeSource(s, routing[i], buffers);
        }

        // -------- FILTER 0 --------
        processFilter(filter0, filter0Params, 0, filter0Route, buffers, sr);

        // -------- FILTER 1 --------
        processFilter(filter1, filter1Params, 1, filter1Route, buffers, sr);

        // combine outputs
        // FINAL VOICE OUTPUT (Serum-style)
        out.left  = buffers.fx.left  + buffers.direct.left;
        out.right = buffers.fx.right + buffers.direct.right;


        if (!env1.isActive())
            active = false;

        return out;
    }
};
