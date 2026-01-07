#pragma once

#include "engine/voice/VoiceRouting.h"
#include "engine/Params/OscParams.h"
#include "engine/Params/NoiseParams.h"

struct VoiceInit {

    static inline void setSerumInitDefaults(
        VoiceRouting& routing,
        OscParams& oscA,
        OscParams& oscB,
        OscParams& oscC,
        OscParams& sub,
        NoiseParams& noise)
    {
        
        // ============================
        // SOURCE ROUTING DEFAULTS
        // ============================

        // OSC A
        routing.sources[0].dest = SourceDest::MAIN;
        routing.sources[0].filterBlend = 0.0f;   // 100% Filter0
        routing.sources[0].bus1Send = 0.0f;
        routing.sources[0].bus2Send = 0.0f;

        // OSC B
        routing.sources[1].dest = SourceDest::MAIN;
        routing.sources[1].filterBlend = 0.0f;
        routing.sources[1].bus1Send = 0.0f;
        routing.sources[1].bus2Send = 0.0f;

        // OSC C
        routing.sources[2].dest = SourceDest::MAIN;
        routing.sources[2].filterBlend = 0.0f;
        routing.sources[2].bus1Send = 0.0f;
        routing.sources[2].bus2Send = 0.0f;

        // SUB
        routing.sources[3].dest = SourceDest::MAIN;
        routing.sources[3].filterBlend = 0.0f;
        routing.sources[3].bus1Send = 0.0f;
        routing.sources[3].bus2Send = 0.0f;

        // NOISE
        routing.sources[4].dest = SourceDest::MAIN;
        routing.sources[4].filterBlend = 0.0f;
        routing.sources[4].bus1Send = 0.0f;
        routing.sources[4].bus2Send = 0.0f;

        // ============================
        // FILTER ROUTING DEFAULTS
        // ============================

        // Filter 0
        routing.filter[0].route = FilterRoute::TO_MAIN;
        routing.filter[0].bus1Send = 0.0f;
        routing.filter[0].bus2Send = 0.0f;

        // Filter 1
        routing.filter[1].route = FilterRoute::TO_MAIN;
        routing.filter[1].bus1Send = 0.0f;
        routing.filter[1].bus2Send = 0.0f;

        // ============================
        // BUS ROUTING DEFAULTS
        // ============================

        routing.busRoute[0] = BusRoute::TO_MAIN; // BUS1 → MAIN
        routing.busRoute[1] = BusRoute::TO_MAIN; // BUS2 → MAIN

        // ============================
        // AUDIBILITY / LEVEL DEFAULTS
        // ============================

            /*
            // OSC A audible
            oscA.levelBase = 0.75f;

            // Everything else muted (but routed) (remember to enable)
            oscB.levelBase = 0.75f;
            oscC.levelBase = 0.75f;
            sub.levelBase  = 0.75f;
            noise.levelBase = dbToLin(-24.0f);
            */
        // ============================
        // FREEZE GRAPH
        // ============================

        routing.compile();
    }
};
