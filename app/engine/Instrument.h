#pragma once

#include <atomic>
#include <vector>

#include "engine/Effects.h"
#include "engine/Lfo.h"
#include "engine/Params.h"
#include "engine/Shared.h"
#include "engine/Voice.h"
#include "engine/Wavetable.h"

// Everything the UI, MIDI and audio threads share. Owned by the app.
struct SharedState {
    ParamStore   params;
    ModSlotStore mods;
    Telemetry    telemetry;
    EventQueue   uiEvents;      // producer: UI thread (mouse piano, computer keys)
    EventQueue   midiEvents;    // producer: MIDI callback thread
    std::atomic<const WavetableSet*> wavetables[2]{};   // owned by the UI side
};

// The synthesizer. Only process() runs on the audio thread; everything it
// touches is preallocated in prepare().
class Instrument {
public:
    explicit Instrument(SharedState& shared) : s(shared) {}

    void prepare(float sampleRate, int maxBlock = 2048);
    float sampleRate() const { return sr; }

    // Audio thread: drains event queues, renders, applies FX, publishes telemetry.
    void process(float* outL, float* outR, int frames);

    // Direct event entry (audio thread / offline rendering).
    void handleEvent(const Event& e);
    void allNotesOff();

private:
    SharedState& s;
    float sr = 48000.0f;
    int maxBlock = 2048;

    Voice voices[MAX_VOICES];
    uint64_t noteCounter = 0;

    // per-block snapshots
    float norm[NUM_PARAMS] = {};
    float value[NUM_PARAMS] = {};
    float gvalue[NUM_PARAMS] = {};     // globals after modulation
    bool  modulated[NUM_PARAMS] = {};
    ModSlotData slots[NUM_MOD_SLOTS];
    int numVoiceSlots = 0;
    ModSlotData globalSlots[NUM_MOD_SLOTS];
    int numGlobalSlots = 0;

    VoiceContext ctx;
    LfoState freeLfo[4];

    // performance state
    float modWheel = 0.0f, aftertouch = 0.0f, pitchBend = 0.0f;
    bool  sustainPedal = false;
    bool  keyDown[128] = {};
    uint8_t heldStack[128] = {};
    int   heldCount = 0;
    float lastPitch = -1.0f;
    int   displayVoice = -1;

    // effects
    DistortionFX dist;
    ChorusFX     chorus;
    DelayFX      delay;
    ReverbFX     reverb;
    Limiter      limiter;
    dsp::Smoother masterGain;
    std::vector<float> busL, busR;
    bool fxWasOn[4] = {};

    void noteOn(int note, float vel);
    void noteOff(int note);
    void releaseVoice(Voice& v);
    int  pickVoice();
    void snapshot();
    void updateGlobalModulation();
    void advanceFreeLfos(int n);
    void renderFx(float* l, float* r, int n);
    void publish(const float* l, const float* r, int n, double busySeconds);

    float gv(P p) const { return gvalue[(int)p]; }
};
