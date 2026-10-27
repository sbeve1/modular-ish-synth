#pragma once

#include <array>

#include "engine/voice/Voice.h"
#include "core/Sample.h"
#include "audio/Wavetable.h"
#include "audio/WavetableFactory.h"
#include "audio/fx/FXChain.h"
#include "routing/RoutingController.h"
#include "core/CONSTANTS.h"

class Synth {
public:
    ModMatrix fxMod;

    std::array<Voice, NUM_VOICES> voices;

    int nextNoteId = 0;

    // ---- GLOBAL PARAM BLOCKS ----
    OscParams oscAParams;
    OscParams oscBParams;
    OscParams oscCParams;
    OscParams subParams;
    NoiseParams noiseParams;

    Wavetable basicWavetable;

    // ===== GLOBAL FX =====
    FXChain bus1FX;
    FXChain bus2FX;
    FXChain mainFX;

    Synth();
    void prepare(float sampleRate);

    void noteOn(int note, float velocity);
    void noteOff(int note);
    Sample process(float sampleRate);
    void setSourceDest(SourceID src, SourceDest dest);
    void setSourceBusSend(SourceID src, BusID bus, float amount);
    void setSourceEnabled(SourceID src, bool enabled);
    void setFilterEnabled(int filterIndex, bool enabled);
    void rebuildFXModulation();

};
