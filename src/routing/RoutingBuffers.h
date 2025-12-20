#pragma once

#include "core/Sample.h"
#include <array>

// Per-voice audio buses (cleared every sample)
struct RoutingBuffers {
    // Inputs to filters
    std::array<Sample, 2> filterInput{};  // [0] → Filter0, [1] → Filter1

    // Post-filter / bypass buses
    Sample main{};    // goes to FX
    Sample direct{};  // bypasses FX

    inline void clear() {
        filterInput[0] = {};
        filterInput[1] = {};
        main   = {};
        direct = {};
    }
};
