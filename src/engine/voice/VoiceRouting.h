#pragma once

#include <array>
#include <algorithm>

#include "routing/Routing.h"
#include "engine/voice/VoiceGraph.h"

// ============================
// VoiceRouting
// ============================
struct VoiceRouting {

    // Per-source routing table (bounded routes per source)
    std::array<SourceRouting, NUM_SOURCES> routing{};

    // Per-voice routing buffers (filter inputs, FX, Direct)
    VoiceRoutingBuffers buffers;

    // High-level graph config (UI / preset driven)
    std::array<SourceRouteConfig, NUM_SOURCES> sourceRoutes{};
    FilterRoute filter0Route = FilterRoute::TO_FX;
    FilterRoute filter1Route = FilterRoute::TO_FX;

    // ----------------------------
    // Graph → routing tables
    // ----------------------------
    inline void configureSourceRoutingFromGraph(int src)
    {
        routing[src].clear();
        const auto& cfg = sourceRoutes[src];

        switch (cfg.dest) {

            case SourceDest::NONE:
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

    inline void applyRoutingGraph()
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

    // ----------------------------
    // SOURCE ROUTING API
    // ----------------------------
    inline void setSourceOff(int src)
    {
        sourceRoutes[src].dest = SourceDest::NONE;
        sourceRoutes[src].filterBlend = 0.0f;
    }

    inline void setSourceToFilter(int src, float blend)
    {
        sourceRoutes[src].dest = SourceDest::FILTER;
        sourceRoutes[src].filterBlend = std::clamp(blend, 0.0f, 1.0f);
    }

    inline void setSourceToFX(int src)
    {
        sourceRoutes[src].dest = SourceDest::FX;
    }

    inline void setSourceToDirect(int src)
    {
        sourceRoutes[src].dest = SourceDest::DIRECT;
    }

    // ----------------------------
    // FILTER ROUTING API
    // ----------------------------
    inline void setFilterOff(int idx)
    {
        (idx == 0) ? filter0Route = FilterRoute::NONE
                   : filter1Route = FilterRoute::NONE;
    }

    inline void setFilterToFX(int idx)
    {
        (idx == 0) ? filter0Route = FilterRoute::TO_FX
                   : filter1Route = FilterRoute::TO_FX;
    }

    inline void setFilterToDirect(int idx)
    {
        (idx == 0) ? filter0Route = FilterRoute::TO_DIRECT
                   : filter1Route = FilterRoute::TO_DIRECT;
    }

    inline void chainFilterToOther(int idx)
    {
        (idx == 0) ? filter0Route = FilterRoute::TO_OTHER_FILTER
                   : filter1Route = FilterRoute::TO_OTHER_FILTER;
    }
};
