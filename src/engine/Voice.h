#pragma once

#include <array>
#include <cstdlib>

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Wavetable.h"
#include "audio/Filter.h"

#include "engine/ModMatrix.h"
#include "engine/VoiceSources.h"

#include "control/Envelope.h"
#include "control/LFO.h"

#include "control/performance/NoteModule.h"
#include "control/performance/VelocityModule.h"
#include "control/performance/ModWheelModule.h"
#include "control/performance/AftertouchModule.h"
#include "control/performance/PitchBendModule.h"
#include "control/performance/RandomPerVoiceModule.h"
#include "control/performance/RandomPerNoteModule.h"

#include "routing/Routing.h"
#include "routing/RoutingBuffers.h"

struct Voice {

    bool active = false;
    int  midiNote = -1;

    // ============================
    // SOURCES
    // ============================
    Oscillator oscA;
    Oscillator oscB;
    Oscillator oscC;
    Oscillator sub;
    Noise      noise;

    // ============================
    // FILTERS
    // ============================
    Filter filter0;
    Filter filter1;

    // ============================
    // CONTROL
    // ============================
    Envelope env1;
    LFO      lfo1;

    // ============================
    // PERFORMANCE MODS
    // ============================
    NoteModule           noteMod;
    VelocityModule       velocityMod;
    ModWheelModule       modWheelMod;
    AftertouchModule     aftertouchMod;
    PitchBendModule      pitchBendMod;
    RandomPerVoiceModule randomVoiceMod;
    RandomPerNoteModule  randomNoteMod;

    float noteModValue = 0.0f;

    // ============================
    // MOD MATRIX
    // ============================
    ModMatrix mod;

    // ============================
    // SERUM-STYLE ROUTING
    // ============================
    enum class SourceMode {
        NONE,
        FILTER,
        MAIN,
        DIRECT
    };

    std::array<SourceMode, NUM_SOURCES> mode{};
    std::array<float, NUM_SOURCES>      filterBlend{};
    RoutingBuffers buffers;

    // ============================
    // START NOTE
    // ============================
    void start(int note, float velocity, const Wavetable* wt)
    {
        active   = true;
        midiNote = note;

        oscA.wavetable = wt;
        oscB.wavetable = wt;
        oscC.wavetable = wt;
        sub.wavetable  = wt;

        float f = oscA.midiNoteToFreq(note);
        oscA.freqBase = f;
        oscB.freqBase = f;
        oscC.freqBase = f;
        sub.freqBase  = f;
        sub.semitoneBase = -12.0f;

        oscA.resetModValues();
        oscB.resetModValues();
        oscC.resetModValues();
        sub.resetModValues();
        noise.resetModValues();

        env1.noteOn(note, velocity);
        lfo1.retrigger();

        mod.clearAll();

        noteModValue = float(note - 60);
        noteMod.midiNote = &noteModValue;
        velocityMod.value = velocity;

        randomVoiceMod.value = (float(rand()) / RAND_MAX) * 2.0f - 1.0f;
        randomNoteMod.value  = (float(rand()) / RAND_MAX) * 2.0f - 1.0f;

        mod.registerSource(ModSourceID::ENV1, &env1);
        mod.registerSource(ModSourceID::LFO1, &lfo1);
        mod.registerSource(ModSourceID::NOTE, &noteMod);
        mod.registerSource(ModSourceID::VELOCITY, &velocityMod);

        // ============================
        // SERUM INIT PATCH
        // ============================
        mode = {
            SourceMode::FILTER,
            SourceMode::NONE,
            SourceMode::NONE,
            SourceMode::NONE,
            SourceMode::NONE
        };

        for (int i = 0; i < (int)NUM_SOURCES; ++i)
            filterBlend[i] = 0.0f;
    }

    void stop() { env1.noteOff(); }

    // ============================
    // PROCESS
    // ============================
    Sample process(float sr) {
        Sample out{};
        out.left = 0.0f;
        out.right = 0.0f;

        if (!active)
            return out;

        buffers.clear();
        mod.process(sr);

        VoiceSources src;
        src.oscA  = oscA.process(sr, {});
        src.oscB  = oscB.process(sr, {});
        src.oscC  = oscC.process(sr, {});
        src.sub   = sub.process(sr, {});
        src.noise = noise.process(sr, {});

        const Sample sources[NUM_SOURCES] = {
            src.oscA, src.oscB, src.oscC, src.sub, src.noise
        };

        // -------- source routing --------
        for (int i = 0; i < (int)NUM_SOURCES; ++i) {
            const Sample& s = sources[i];

            switch (mode[i]) {
                case SourceMode::NONE:
                    break;

                case SourceMode::FILTER:
                {
                    float a = 1.0f - filterBlend[i];
                    float b = filterBlend[i];

                    buffers.filterInput[0].left  += s.left  * a;
                    buffers.filterInput[0].right += s.right * a;

                    buffers.filterInput[1].left  += s.left  * b;
                    buffers.filterInput[1].right += s.right * b;
                }
                break;

                case SourceMode::MAIN:
                case SourceMode::DIRECT:
                    out.left  += s.left;
                    out.right += s.right;
                    break;
            }
        }

        // -------- filters --------
        Sample f0 = filter0.process(sr, buffers.filterInput[0]);
        Sample f1 = filter1.process(sr, buffers.filterInput[1]);

        out.left  += f0.left  + f1.left;
        out.right += f0.right + f1.right;

        // -------- amp envelope --------
        float env = env1.process(sr);
        out.left  *= env;
        out.right *= env;

        if (!env1.isActive())
            active = false;

        return out;
    }
};
