#pragma once
#include <vector>
#include <cassert>

struct Wavetable {
    int tableSize = 2048;      // samples per frame (must be power of two)
    int numFrames = 0;
    std::vector<float> data;

    float sample(int frame, int index) const {
#ifndef NDEBUG
        assert((tableSize & (tableSize - 1)) == 0 &&
               "Wavetable::tableSize must be power of two");
#endif
        return data[frame * tableSize + index];
    }
};
