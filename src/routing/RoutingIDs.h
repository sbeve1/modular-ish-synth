#pragma once

enum class SourceID : int {
    OSC_A = 0,
    OSC_B = 1,
    OSC_C = 2,
    SUB   = 3,
    NOISE = 4
};

enum class FilterID : int {
    FILTER0 = 0,
    FILTER1 = 1
};

enum class BusID : int {
    BUS1 = 0,
    BUS2 = 1
};
