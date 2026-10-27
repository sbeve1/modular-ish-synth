#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Small real-time DSP helpers. Everything here is allocation-free.

namespace dsp {

constexpr float PI     = 3.14159265358979f;
constexpr float TWO_PI = 6.28318530717959f;

inline float fastTanh(float x)
{
    x = std::clamp(x, -3.0f, 3.0f);
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float killDenormal(float x) { return std::fabs(x) < 1e-15f ? 0.0f : x; }

// ---- fast math for per-sample paths (no libm calls) ----

// 2^x, x >= -126. Split at the nearest integer so the fraction is within
// +-0.5; degree-5 polynomial there: max rel. error ~1.5e-6, continuous.
inline float fastExp2(float x)
{
    x = std::max(x, -126.0f);
    float fi = std::floor(x + 0.5f), f = x - fi;
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    union { uint32_t i; float f; } u;
    u.i = (uint32_t)((int)fi + 127) << 23;
    return p * u.f;
}

// 2^x for x <= 0 only (e.g. p^e with p < 1): same accuracy as fastExp2,
// rounding by truncation instead of floor.
inline float fastExp2Neg(float x)
{
    x = std::max(x, -126.0f);
    int fi = (int)(x - 0.5f);   // nearest integer for x <= 0
    float f = x - (float)fi;
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    union { uint32_t i; float f; } u;
    u.i = (uint32_t)(fi + 127) << 23;
    return p * u.f;
}

// log2(x), x > 0. Exponent from the bits, mantissa via the atanh series: error ~1e-6.
inline float fastLog2(float x)
{
    union { float f; uint32_t i; } u{ x };
    int e = (int)((u.i >> 23) & 255) - 127;
    u.i = (u.i & 0x007FFFFFu) | 0x3F800000u;
    float m = u.f, t = (m - 1.0f) / (m + 1.0f), t2 = t * t;
    float ln = 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * (0.14285714f + t2 * 0.11111111f))));
    return (float)e + ln * 1.44269504f;
}

// sin(x) for any x: range-reduce to [-pi/2, pi/2], odd polynomial (error ~1e-6).
inline float fastSin(float x)
{
    x -= TWO_PI * std::floor(x * (1.0f / TWO_PI) + 0.5f);
    if (x > 0.5f * PI)       x = PI - x;
    else if (x < -0.5f * PI) x = -PI - x;
    float x2 = x * x;
    return x * (1.0f + x2 * (-0.16666667f + x2 * (0.0083333310f + x2 * (-0.00019840874f + x2 * 2.7525562e-6f))));
}

inline float fastDbToGain(float db) { return fastExp2(db * 0.16609640f); }        // 10^(db/20)
inline float fastGainToDb(float g)  { return 6.02059991f * fastLog2(std::max(g, 1e-9f)); }

inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g)  { return 20.0f * std::log10(std::max(g, 1e-9f)); }

inline float midiToHz(float note) { return 440.0f * std::exp2((note - 69.0f) * (1.0f / 12.0f)); }

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Equal-power pan, -1..1
inline void panGains(float pan, float& l, float& r)
{
    float a = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * PI;
    l = std::cos(a);
    r = std::sin(a);
}

struct Rng {
    uint32_t s = 0x12345678u;
    explicit Rng(uint32_t seed = 0x12345678u) : s(seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()     { return (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    inline float bi()      { return uni() * 2.0f - 1.0f; }                    // -1..1
};

// PolyBLEP residual for band-limited saw/square edges
inline float polyBlep(float t, float dt)
{
    if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct OnePole {
    float z = 0.0f, a = 0.0f;
    void setCutoff(float hz, float sr) { a = std::exp(-TWO_PI * std::min(hz, sr * 0.49f) / sr); }
    inline float lp(float x) { z = x + a * (z - x); return z; }
    inline float hp(float x) { return x - lp(x); }
    void reset() { z = 0.0f; }
};

// Smooths a control value per sample (for FX params changing at block rate)
struct Smoother {
    float cur = 0.0f, coef = 0.0f;
    void setTime(float seconds, float sr) { coef = std::exp(-1.0f / std::max(seconds * sr, 1.0f)); }
    void snap(float v) { cur = v; }
    inline float next(float target) { cur = target + coef * (cur - target); return cur; }
};

// Zero-delay-feedback state variable filter (Simper/Cytomic), stereo.
struct Svf {
    float g = 0.0f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1[2] = {}, ic2[2] = {};

    void set(float cutoff, float kDamp, float sr)
    {
        cutoff = std::clamp(cutoff, 10.0f, sr * 0.47f);
        g  = std::tan(PI * cutoff / sr);
        k  = kDamp;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // returns lp, bp, hp through refs
    inline void tick(int ch, float v0, float& lp, float& bp, float& hp)
    {
        float v3 = v0 - ic2[ch];
        float v1 = a1 * ic1[ch] + a2 * v3;
        float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }

    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; }
};

// Magnitude response of one SVF stage at frequency f (for the UI curve)
inline float svfMagnitude(int mode /*0 lp,1 bp,2 hp,3 notch,4 peak*/, float f, float cutoff, float k)
{
    float w = f / cutoff;            // normalized (analog prototype)
    float re = 1.0f - w * w, im = k * w;
    float den = std::sqrt(re * re + im * im);
    switch (mode) {
        case 0: return 1.0f / den;
        case 1: return w / den;
        case 2: return (w * w) / den;
        case 3: return std::fabs(1.0f - w * w) / den;
        default: return (1.0f + w * w) / den;   // peak = lp - hp
    }
}

} // namespace dsp
