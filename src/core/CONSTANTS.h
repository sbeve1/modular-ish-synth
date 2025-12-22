#pragma once
#include <cmath>
#include <cstdio>

// ============================================================
// Math constants
// ============================================================
constexpr float PI       = 3.14159265358979323846f;
constexpr float TWO_PI   = 6.28318530717958647692f;
constexpr float HALF_PI  = 1.57079632679489661923f;
constexpr float INV_PI   = 0.31830988618379067154f;
constexpr float INV_2PI  = 0.15915494309189533577f;

constexpr float EPSILON  = 1e-12f;

// ============================================================
// Frequency limits / safety
// ============================================================
constexpr float MIN_FREQ       = 20.0f;
constexpr float MAX_FREQ       = 20000.0f;
constexpr float NYQUIST_SCALE  = 0.49f;

// ============================================================
// Oscillator normalization (RMS matching)
// ============================================================
constexpr float OSC_RMS_SINE     = 1.0f;
constexpr float OSC_RMS_TRI_SAW  = 1.22f;   // 1 / 0.577
constexpr float OSC_RMS_SQUARE   = 0.707f;  // RMS match to sine

// ============================================================
// Saturation / shaping
// ============================================================
constexpr float SAT_SOFT_K   = 1.0f;
constexpr float SAT_TANH_K   = 1.5f;
constexpr float SAT_CLIP_K   = 0.999f;

// ============================================================
// Filter tuning / normalization
// ============================================================
constexpr float FILTER_RESONANCE_MIN   = 0.5f;
constexpr float FILTER_RESONANCE_RANGE = 2.5f;
constexpr float FILTER_RESONANCE_K     = 2.5f;

// ============================================================
// Envelope tuning
// ============================================================
constexpr float ENV_MIN_TIME   = 0.001f;
constexpr float ENV_MAX_TIME   = 20.0f;
constexpr float ENV_EXP_CURVE  = 5.0f;

// ============================================================
// Gain staging (engine-level)
// ============================================================
constexpr float VOICE_GAIN_NORM = 0.353553f;   // 8 voices → 1/sqrt(8)
constexpr float MASTER_GAIN     = 2.50f;
constexpr float DB_MINUS_10     = 0.31622777f;
constexpr float DB_MINUS_20     = 0.1f;

// ============================================================
// Oversampling configuration
// ============================================================
constexpr int OVERSAMPLE_2X   = 2;
constexpr int OVERSAMPLE_4X   = 4;
constexpr int MAX_OVERSAMPLE  = 4;

// ============================================================
// Lookup tables
// ============================================================

// ---- Sine table (oscillators, LFOs)
constexpr int SINE_TABLE_SIZE = 2048;
static_assert((SINE_TABLE_SIZE & (SINE_TABLE_SIZE - 1)) == 0,
              "SINE_TABLE_SIZE must be power of two");

extern float SINE_TABLE[SINE_TABLE_SIZE];

// ---- Tanh / saturation table (optional use)
constexpr int TANH_TABLE_SIZE = 2048;
static_assert((TANH_TABLE_SIZE & (TANH_TABLE_SIZE - 1)) == 0,
              "TANH_TABLE_SIZE must be power of two");

extern float TANH_TABLE[TANH_TABLE_SIZE];

// ---- Filter normalization curves (future-proofing)
constexpr int FILTER_NORM_TABLE_SIZE = 128;
extern float FILTER_NORM_TABLE[FILTER_NORM_TABLE_SIZE];

// ============================================================
// Initialization
// ============================================================
void initDSPTables();
void initLFOShape();

inline float dbToLin(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }