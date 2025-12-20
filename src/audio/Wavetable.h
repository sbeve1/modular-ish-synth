#pragma once
#include <vector>

struct Wavetable {
    int tableSize = 2048;      // samples per frame
    int numFrames = 0;         // number of waveforms
    std::vector<float> data;   // size = tableSize * numFrames

    float sample(int frame, int index) const {
        return data[frame * tableSize + index];
    }
};
