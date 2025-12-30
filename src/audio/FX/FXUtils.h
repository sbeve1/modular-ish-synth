#pragma once
#include <cmath>

inline float serumSat(float x)
{
    // gentle tanh-based saturation
    return std::tanh(x * 0.85f);
}

inline float serumLimiter(float x)
{
    // final soft limiter — tanh maps to [-1,1]
    return std::tanh(x);
}

inline float killDenorm(float x)
{
    return (fabsf(x) < 1e-20f) ? 0.0f : x;
}


// global constants for Serum-style behavior
constexpr float SERUM_FX_SEND_GAIN = 0.12f; // ~ -18.4 dB
constexpr float SERUM_BUS_PRE_SAT   = 0.95f; // slight attenuation before bus sat
constexpr float SERUM_MASTER_GAIN   = 0.85f; // reasonable master headroom
