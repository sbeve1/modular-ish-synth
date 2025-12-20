#pragma once
#include "core/Sample.h"
#include <array>

struct RoutingBuffers {
    std::array<Sample, 2> filterInput{};
    Sample main{};
    Sample direct{};

    inline void clear() {
        filterInput[0] = {};
        filterInput[1] = {};
        main = {};
        direct = {};
    }
};
