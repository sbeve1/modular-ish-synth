#include "core/CONSTANTS.h"

// ============================================================
// Table storage
// ============================================================
float SINE_TABLE[SINE_TABLE_SIZE];
float TANH_TABLE[TANH_TABLE_SIZE];
float FILTER_NORM_TABLE[FILTER_NORM_TABLE_SIZE];

// ============================================================
// Initialization (call once at startup)
// ============================================================
void initDSPTables()
{
    // ---- Sine table
    for (int i = 0; i < SINE_TABLE_SIZE; ++i) {
        SINE_TABLE[i] = sinf(TWO_PI * i / SINE_TABLE_SIZE);
    }

    // ---- Tanh table
    for (int i = 0; i < TANH_TABLE_SIZE; ++i) {
        float x = 2.0f * i / (TANH_TABLE_SIZE - 1) - 1.0f;
        TANH_TABLE[i] = tanhf(x * SAT_TANH_K);
    }

    // ---- Filter normalization curve
    for (int i = 0; i < FILTER_NORM_TABLE_SIZE; ++i) {
        float r = (float)i / (FILTER_NORM_TABLE_SIZE - 1);
        FILTER_NORM_TABLE[i] = 1.0f / (1.0f + r * FILTER_RESONANCE_K);
    }
}
