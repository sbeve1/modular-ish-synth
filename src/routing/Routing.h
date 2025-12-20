#pragma once
#include <array>

constexpr int NUM_SOURCES = 5;
constexpr int NUM_FILTERS = 2;

enum class LayerTarget {
    FILTER0,
    FILTER1,
    BUS0, 
    BUS1,
    MAIN,
    DIRECT
};

struct Route {
    LayerTarget target;
    float gain;
};

struct SourceRouting {
    std::array<Route, 2> routes{};
    int count = 0;

    inline void clear() { count = 0; }

    inline void add(LayerTarget t, float g) {
        routes[count++] = { t, g };
    }
};
