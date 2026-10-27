#pragma once

#include <cstdint>

#include "control/Envelope.h"
#include "engine/Dsp.h"
#include "engine/Lfo.h"
#include "engine/Params.h"
#include "engine/Shared.h"
#include "engine/Wavetable.h"

constexpr int MAX_VOICES   = 16;
constexpr int UNISON_MAX   = 16;
constexpr int CONTROL_RATE = 16;   // samples per modulation update

// Everything a voice needs from the instrument for one control block.
struct VoiceContext {
    float sampleRate = 48000.0f;
    float tempo = 120.0f;

    const float* norm = nullptr;        // base normalized values [NUM_PARAMS]
    const float* value = nullptr;       // base plain values [NUM_PARAMS]
    const bool*  modulated = nullptr;   // param is a target of an active slot

    const ModSlotData* slots = nullptr; // active slots only
    int numSlots = 0;

    float globalSrc[NUM_MOD_SRC] = {};  // mod wheel, aftertouch, bend, macros
    const LfoState* freeLfo = nullptr;  // [4] shared free-running LFOs

    const WavetableSet* wavetable[2] = {};
    const LfoCurveTable* lfoCurve[4] = {};
};

class Voice {
public:
    // ---- state visible to the instrument ----
    bool     active = false;
    bool     gate = false;        // key (or sustain pedal) holding the note
    bool     sustained = false;   // released while the pedal was down
    int      note = 60;
    float    velocity = 1.0f;
    uint64_t startOrder = 0;      // for stealing / newest-voice display

    float src[NUM_MOD_SRC] = {};          // last computed source values
    float modOff[NUM_PARAMS] = {};        // normalized offsets from the matrix
    float modValue[NUM_PARAMS] = {};      // plain value of each modulated param, once per block
    float modMin[NUM_PARAMS], modMax[NUM_PARAMS];   // swept range since the instrument last read it

    Voice() { resetModRange(); }
    void resetModRange() { for (int i = 0; i < NUM_PARAMS; ++i) { modMin[i] = 2.0f; modMax[i] = -1.0f; } }

    void start(int note, float velocity, float startPitch, uint64_t order, const VoiceContext& ctx, bool retrigger);
    void release();
    void glideTo(int newNote) { note = newNote; targetPitch = (float)newNote; }
    float amplitude() const { return ampCur; }

    // Renders n <= CONTROL_RATE samples, adding into outL/outR.
    void render(const VoiceContext& ctx, float* outL, float* outR, int n);

    float currentPitch() const { return pitch; }
    const Envelope& env(int i) const { return envs[i]; }
    float lfoPhase(int i, const VoiceContext& ctx) const;
    float wtPos(int osc) const { return oscState[osc].pos; }

private:
    struct OscState {
        float phase[UNISON_MAX] = {};
        float pos = 0.0f;   // effective WT position, for display
        // cached unison layout (recomputed only when its inputs change)
        int   cUni = -1;
        float cDetune = -1.0f, cBlend = -1.0f, cWidth = -2.0f, cPan = -2.0f, cLevel = -1.0f;
        float ratio[UNISON_MAX] = {}, gl[UNISON_MAX] = {}, gr[UNISON_MAX] = {};
        float maxRatio = 1.0f;
    };

    float pitch = 60.0f, targetPitch = 60.0f;
    float ampCur = 0.0f;
    float timeOn = 0.0f;
    float randomValue = 0.0f;

    Envelope envs[3];
    LfoState lfo[4];
    OscState oscState[2];

    float subPhase = 0.0f;
    float pink[7] = {};
    float brown = 0.0f;
    dsp::Rng rng;

    dsp::Svf svf[2][2];          // [filter][stage]
    int lastFilterType[2] = { -1, -1 };

    float eff(const VoiceContext& ctx, P p) const;
    void  updateModulation(const VoiceContext& ctx);
    void  updateSources(const VoiceContext& ctx, int n);
    void  renderOsc(const VoiceContext& ctx, int osc, float freqBase, float* l, float* r, float* fl, float* fr, int n);
    void  runFilter(const VoiceContext& ctx, int f, float* l, float* r, int n);
};
