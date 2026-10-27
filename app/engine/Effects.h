#pragma once

#include <vector>

#include "engine/Dsp.h"

// Global effects. prepare() allocates; process() never does.
// Each process() works in place on stereo buffers and applies its own mix.

struct DelayLine {
    std::vector<float> buf;
    int mask = 0, w = 0;

    void allocate(int minSamples)
    {
        int n = 1;
        while (n < minSamples) n <<= 1;
        buf.assign(n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
    inline void push(float x) { buf[w] = x; w = (w + 1) & mask; }
    inline float tap(float delaySamples) const   // linear interpolation
    {
        float r = (float)w - delaySamples - 1.0f;
        while (r < 0.0f) r += (float)(mask + 1);
        int i0 = (int)r;
        float f = r - (float)i0;
        return buf[i0 & mask] + f * (buf[(i0 + 1) & mask] - buf[i0 & mask]);
    }
    inline float tapInt(int delaySamples) const { return buf[(w - delaySamples - 1) & mask]; }
};

struct DistortionFX {
    struct Params { int mode; float drive, tone, mix; };
    float sr = 48000.0f;
    dsp::OnePole toneL, toneR, dcL, dcR;
    float crushHold[2] = {}, crushCount = 0.0f;

    void prepare(float sampleRate) { sr = sampleRate; }
    void reset() { toneL.reset(); toneR.reset(); dcL.reset(); dcR.reset(); }
    void process(float* l, float* r, int n, const Params& p);
};

struct ChorusFX {
    struct Params { float rate, depth, feedback, mix; };
    float sr = 48000.0f;
    DelayLine dl[2];
    float phase = 0.0f, fb[2] = {};

    void prepare(float sampleRate);
    void reset() { dl[0].clear(); dl[1].clear(); fb[0] = fb[1] = 0.0f; }
    void process(float* l, float* r, int n, const Params& p);
};

struct DelayFX {
    struct Params { float timeSec, feedback, tone, mix; bool pingPong; };
    float sr = 48000.0f;
    DelayLine dl[2];
    dsp::Smoother time;
    dsp::OnePole lp[2], hp[2];

    void prepare(float sampleRate);
    void reset() { dl[0].clear(); dl[1].clear(); }
    void process(float* l, float* r, int n, const Params& p);
};

// 8-line feedback delay network: input diffusers, Hadamard mixing,
// per-line damping, RT60-derived feedback gains, slow modulation.
struct ReverbFX {
    struct Params { float size, decay, damping, preDelayMs, width, mix; };
    static constexpr int N = 8;

    float sr = 48000.0f;
    DelayLine pre;
    DelayLine diff[4];
    DelayLine line[N];
    float lpState[N] = {};
    float modPhase = 0.0f;

    void prepare(float sampleRate);
    void reset();
    void process(float* l, float* r, int n, const Params& p);
};

struct Limiter {
    float sr = 48000.0f, env = 0.0f, gain = 1.0f;
    void prepare(float sampleRate) { sr = sampleRate; env = 0.0f; gain = 1.0f; }
    void process(float* l, float* r, int n, float ceiling);
    float gainReduction() const { return gain; }
};
