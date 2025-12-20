#pragma once
#include "audio/Wavetable.h"
#include "core/CONSTANTS.h"
#include <cmath>

namespace WavetableFactory {

inline Wavetable makeBasicShapes(int tableSize = 2048)
{
    Wavetable wt;
    wt.tableSize = tableSize;
    wt.numFrames = 4;
    wt.data.resize(tableSize * wt.numFrames);

    for (int i = 0; i < tableSize; ++i)
    {
        float phase = (float)i / tableSize * TWO_PI;

        // Frame 0: Sine
        wt.data[0 * tableSize + i] = sinf(phase);

        // Frame 1: Triangle
        wt.data[1 * tableSize + i] =
            2.0f * fabsf(phase * INV_PI - 1.0f) - 1.0f;

        // Frame 2: Saw
        wt.data[2 * tableSize + i] =
            2.0f * (phase * INV_2PI) - 1.0f;

        // Frame 3: Square
        wt.data[3 * tableSize + i] =
            (phase < PI ? 1.0f : -1.0f);
    }

    return wt;
}

}
