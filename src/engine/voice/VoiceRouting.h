#pragma once
#include <array>
#include <algorithm>

#include "routing/Routing.h"
#include "engine/voice/VoiceGraph.h"

struct VoiceRouting {

    // Compiled
    std::array<SourceRouting, NUM_SOURCES> routing{};
    VoiceRoutingBuffers buffers;

    // Graph
    std::array<SourceRouteConfig, NUM_SOURCES> sources{};
    FilterRouteConfig filter[2]{};

    BusRoute busRoute[2] = {
        BusRoute::TO_MAIN,
        BusRoute::TO_MAIN
    };

    inline void resolveCycles()
    {
        if (filter[0].route == FilterRoute::TO_OTHER_FILTER &&
            filter[1].route == FilterRoute::TO_OTHER_FILTER)
            filter[1].route = FilterRoute::TO_MAIN;

        if (busRoute[0] == BusRoute::TO_OTHER_BUS &&
            busRoute[1] == BusRoute::TO_OTHER_BUS)
            busRoute[1] = BusRoute::TO_MAIN;
    }

    inline void compile()
    {
        resolveCycles();

        for (auto& r : routing)
            r.clear();

        for (int i = 0; i < NUM_SOURCES; ++i)
            compileSource(i);
    }

private:
    inline void compileSource(int i)
    {
        auto& out = routing[i];
        const auto& s = sources[i];

        if (s.dest == SourceDest::FILTER) {
            float b = std::clamp(s.filterBlend, 0.0f, 1.0f);
            out.add(LayerTarget::FILTER0, 1.0f - b);
            out.add(LayerTarget::FILTER1, b);
        } else if (s.dest == SourceDest::MAIN) {
            out.add(LayerTarget::MAIN, 1.0f);
        } else if (s.dest == SourceDest::DIRECT) {
            out.add(LayerTarget::DIRECT, 1.0f);
        }

        out.add(LayerTarget::BUS1, std::clamp(s.bus1Send, 0.0f, 1.0f));
        out.add(LayerTarget::BUS2, std::clamp(s.bus2Send, 0.0f, 1.0f));
    }
};
