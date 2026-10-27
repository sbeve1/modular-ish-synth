#pragma once

#include <cstdint>

#include "core/CONSTANTS.h"
#include "engine/Params/OscParams.h"
#include "engine/Params/NoiseParams.h"
#include "engine/Params/FilterParams.h"
#include "engine/voice/VoiceGraph.h"
#include "routing/Routing.h"

// A complete, allocation-free sound: safe to copy and apply on the audio thread.
struct Patch {
    const char* name = "Init";

    OscParams oscA, oscB, oscC, sub;
    NoiseParams noise;
    FilterParams filter0;

    // Indexed by SourceID. Only OSC A is on by default, straight to main.
    SourceRouteConfig sources[NUM_SOURCES] = {
        { true,  SourceDest::MAIN, 0.0f, 0.0f, 0.0f },
        { false, SourceDest::MAIN, 0.0f, 0.0f, 0.0f },
        { false, SourceDest::MAIN, 0.0f, 0.0f, 0.0f },
        { false, SourceDest::MAIN, 0.0f, 0.0f, 0.0f },
        { false, SourceDest::MAIN, 0.0f, 0.0f, 0.0f },
    };

    // bus sends here are post-filter (bus 1 = reverb, bus 2 = delay)
    FilterRouteConfig filter0Route = { true, FilterRoute::TO_MAIN, 0.0f, 0.0f };

    // amp envelope (seconds, sustain 0..1)
    float attack  = 0.001f;
    float decay   = 1.0f;
    float sustain = 1.0f;
    float release = 0.015f;
};
