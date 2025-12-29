#pragma once
#include <array>
#include "core/Sample.h"

constexpr int NUM_SOURCES = 5;

// Routing targets
enum class LayerTarget {
    FILTER0,
    FILTER1,
    BUS1,
    BUS2,
    MAIN,
    DIRECT
};

struct RoutingRoute {
    LayerTarget target;
    float gain;
};

struct SourceRouting {
    std::array<RoutingRoute, 6> routes{};
    int count = 0;

    inline void clear() { count = 0; }

    inline bool isEmpty() const {
        return count == 0;
    }

    inline void add(LayerTarget t, float g) {
        if (count < (int)routes.size() && g > 0.0f)
            routes[count++] = { t, g };
    }
};

struct VoiceRoutingBuffers {
    std::array<Sample, 2> filterInput{};
    Sample bus1{};
    Sample bus2{};
    Sample main{};
    Sample direct{};

    inline void clear() {
        filterInput[0] = {};
        filterInput[1] = {};
        bus1 = {};
        bus2 = {};
        main = {};
        direct = {};
    }
};

inline void routeSource(
    const Sample& s,
    const SourceRouting& r,
    VoiceRoutingBuffers& buf)
{
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
            case LayerTarget::BUS1:
                buf.bus1.left  += l;
                buf.bus1.right += rgt;
                break;
            case LayerTarget::BUS2:
                buf.bus2.left  += l;
                buf.bus2.right += rgt;
                break;
            case LayerTarget::MAIN:
                buf.main.left  += l;
                buf.main.right += rgt;
                break;
            case LayerTarget::DIRECT:
                buf.direct.left  += l;
                buf.direct.right += rgt;
                break;
        }
    }
}
