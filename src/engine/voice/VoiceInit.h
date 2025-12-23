#pragma once

#include <cstdlib>

#include "engine/OscParams.h"
#include "engine/NoiseParams.h"
#include "engine/FilterParams.h"
#include "core/CONSTANTS.h"
#include "audio/Wavetable.h"
#include "control/Envelope.h"
#include "control/LFO.h"
#include "engine/voice/VoiceRouting.h"

struct VoiceInit {

    static inline void setSerumInitDefaults(VoiceRouting& routing)
    {
        for (int i = 0; i < NUM_SOURCES; ++i) {
            routing.sourceRoutes[i].dest = SourceDest::NONE;
            routing.sourceRoutes[i].filterBlend = 0.0f;
        }

        routing.sourceRoutes[0].dest = SourceDest::FILTER;
        routing.sourceRoutes[0].filterBlend = 0.0f;

        routing.filter0Route = FilterRoute::TO_FX;
        routing.filter1Route = FilterRoute::TO_FX;

        routing.applyRoutingGraph();
    }
};
