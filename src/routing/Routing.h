#pragma once
#include <array>
#include "core/Sample.h"

// Fixed number of sources
constexpr int NUM_SOURCES = 5;   // oscA, oscB, oscC, sub, noise

// Where audio can go
enum class LayerTarget {
    FILTER0,
    FILTER1,
    FX,       // wet bus
    DIRECT    // dry bus
};

// One connection: source → target with gain
struct RoutingRoute {
    LayerTarget target;
    float gain;
};

// All routes for one source (bounded)
struct SourceRouting {
    std::array<RoutingRoute, 2> routes{};
    int count = 0;

    inline void clear() { count = 0; }

    inline void add(LayerTarget t, float g) {
        if (count < (int)routes.size())
            routes[count++] = { t, g };
    }
};

// Per-voice routing buses
struct VoiceRoutingBuffers {
    std::array<Sample, 2> filterInput{}; // [0]=F0, [1]=F1
    Sample fx{};      // wet path (FX input)
    Sample direct{};  // dry bypass

    inline void clear() {
        filterInput[0] = {};
        filterInput[1] = {};
        fx = {};
        direct = {};
    }
};

// Canonical routing function
inline void routeSource(
    const Sample& s,
    const SourceRouting& r,
    VoiceRoutingBuffers& buf)
{
    if (r.count == 0)
        return;
        
    for (int i = 0; i < r.count; ++i) {
        const auto& rt = r.routes[i];
        float l = s.left  * rt.gain;
        float rgt = s.right * rt.gain;

        switch (rt.target) {
            case LayerTarget::FILTER0:
                buf.filterInput[0].left  += l;
                buf.filterInput[0].right += rgt;
                break;
            case LayerTarget::FILTER1:
                buf.filterInput[1].left  += l;
                buf.filterInput[1].right += rgt;
                break;
            case LayerTarget::FX:
                buf.fx.left  += l;
                buf.fx.right += rgt;
                break;
            case LayerTarget::DIRECT:
                buf.direct.left  += l;
                buf.direct.right += rgt;
                break;
        }
    }
}
