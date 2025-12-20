#pragma once
#include <array>

// Number of audio sources per voice
constexpr int NUM_SOURCES = 5;   // oscA, oscB, oscC, sub, noise

// Where a source can send audio
enum class LayerTarget {
    FILTER0,
    FILTER1,
    MAIN,
    DIRECT
};

// One connection: source → target with gain
struct RoutingRoute {
    LayerTarget target;
    float gain;
};

// All routes for one source
struct SourceRouting {
    // Max 2 routes per source (enough for filter blend)
    std::array<RoutingRoute, 2> routes{};
    int count = 0;

    inline void clear() {
        count = 0;
    }

    inline void add(LayerTarget t, float g) {
        if (count < static_cast<int>(routes.size())) {
            routes[count].target = t;
            routes[count].gain   = g;
            ++count;
        }
    }
};
