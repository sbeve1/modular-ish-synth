#pragma once

#include <vector>

#include "engine/Dsp.h"

// Global effects. prepare() allocates; process() never does.
// Each process() works in place on stereo buffers and applies its own mix.

// Power-of-two ring buffer. clear() is O(1): it resets a fill counter and
// taps further back than what has been written since read as silence, so
// switching an effect on never has to wipe megabytes inside the callback.
struct DelayLine {
    std::vector<float> buf;
    int mask = 0, w = 0, filled = 0;

    void allocate(int minSamples)
    {
        int n = 1;
        while (n < minSamples) n <<= 1;
        buf.assign(n, 0.0f);
        mask = n - 1;
        w = 0;
        filled = 0;
    }
    void clear() { filled = 0; }
    inline void push(float x) { buf[w] = x; w = (w + 1) & mask; if (filled <= mask) ++filled; }
    inline float tap(float delaySamples) const   // linear interpolation
    {
        if ((int)delaySamples + 2 > filled) return 0.0f;
        float r = (float)w - delaySamples - 1.0f;
        while (r < 0.0f) r += (float)(mask + 1);
        int i0 = (int)r;
        float f = r - (float)i0;
        return buf[i0 & mask] + f * (buf[(i0 + 1) & mask] - buf[i0 & mask]);
    }
    inline float tapInt(int delaySamples) const
    {
        return delaySamples + 1 > filled ? 0.0f : buf[(w - delaySamples - 1) & mask];
    }
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

// RBJ biquad (transposed direct form II), stereo
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1[2] = {}, z2[2] = {};
    void lowShelf(float f, float gainDb, float sr);
    void highShelf(float f, float gainDb, float sr);
    void peak(float f, float q, float gainDb, float sr);
    inline float tick(int ch, float x)
    {
        float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }
    float magnitudeDb(float f, float sr) const;   // for the UI curve
};

struct EqFX {
    struct Params { float lowDb, lowHz, midDb, midHz, midQ, highDb, highHz; };
    float sr = 48000.0f;
    Biquad band[3];
    void prepare(float sampleRate) { sr = sampleRate; }
    void reset() { for (auto& b : band) b.reset(); }
    void process(float* l, float* r, int n, const Params& p);
};

// Stereo-linked feed-forward compressor with a soft knee.
struct CompressorFX {
    struct Params { float threshDb, ratio, attackMs, releaseMs, makeupDb, mix; };
    float sr = 48000.0f, envDb = -120.0f, grDb = 0.0f;
    void prepare(float sampleRate) { sr = sampleRate; envDb = -120.0f; grDb = 0.0f; }
    void reset() { envDb = -120.0f; grDb = 0.0f; }
    void process(float* l, float* r, int n, const Params& p);
};

// 6-stage all-pass phaser, stereo LFOs 90 degrees apart.
struct PhaserFX {
    struct Params { float rate, depth, feedback, mix; };
    static constexpr int STAGES = 6;
    float sr = 48000.0f, phase = 0.0f;
    float ap[2][STAGES] = {}, fb[2] = {};
    float coef[2] = { -1.0f, -1.0f };   // all-pass coefficient at the end of the last block
    bool  coefValid = false;
    void prepare(float sampleRate) { sr = sampleRate; }
    void reset() { for (auto& c : ap) for (float& s : c) s = 0.0f; fb[0] = fb[1] = 0.0f; coefValid = false; }
    void process(float* l, float* r, int n, const Params& p);
};

struct FlangerFX {
    struct Params { float rate, depth, feedback, mix; };
    float sr = 48000.0f, phase = 0.0f;
    DelayLine dl[2];
    void prepare(float sampleRate);
    void reset() { dl[0].clear(); dl[1].clear(); }
    void process(float* l, float* r, int n, const Params& p);
};

struct Limiter {
    float sr = 48000.0f, env = 0.0f, gain = 1.0f;
    void prepare(float sampleRate) { sr = sampleRate; env = 0.0f; gain = 1.0f; }
    void process(float* l, float* r, int n, float ceiling);
    float gainReduction() const { return gain; }
};
