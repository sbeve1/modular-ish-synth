#include "LFO.h"

float LFO_SHAPE[LFO_TABLE_SIZE];

void initLFOShape()
{
    // Default Serum-style triangle ramp (bipolar, -1 → +1)
    for (int i = 0; i < LFO_TABLE_SIZE; i++) {
        float ph = (float)i / LFO_TABLE_SIZE;

        float t;
        if (ph < 0.5f)
            t = 4.0f * ph - 1.0f;        // rising -1 → +1
        else
            t = 3.0f - 4.0f * ph;        // falling +1 → -1

        LFO_SHAPE[i] = t;
    }
}
