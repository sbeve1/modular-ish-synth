#pragma once

#include <memory>
#include <string>
#include <vector>

// =====================================================================
// Band-limited 8-frame wavetable.
//
// The 8 source frames are what the editor shows and edits. build() FFTs
// each frame and makes WT_LEVELS mip levels; level L keeps harmonics up to
// 1024 >> L, so the oscillator can pick a level whose top harmonic stays
// under Nyquist and never aliases. Each table has a guard sample so
// interpolation needs no wrap check.
// =====================================================================

constexpr int WT_SIZE   = 2048;
constexpr int WT_FRAMES = 8;
constexpr int WT_LEVELS = 11;

struct WavetableSet {
    std::string name;
    std::vector<float> frames;   // [WT_FRAMES][WT_SIZE] source
    std::vector<float> mips;     // [WT_LEVELS][WT_FRAMES][WT_SIZE + 1]

    WavetableSet() : frames((size_t)WT_FRAMES * WT_SIZE, 0.0f) {}

    float*       frame(int f)       { return &frames[(size_t)f * WT_SIZE]; }
    const float* frame(int f) const { return &frames[(size_t)f * WT_SIZE]; }

    const float* table(int level, int f) const
    {
        return &mips[((size_t)level * WT_FRAMES + f) * (WT_SIZE + 1)];
    }

    // Normalizes each frame, removes DC and builds the mip levels.
    void build();
};

// Mip level whose harmonics stay below Nyquist for this frequency.
inline int wtLevelFor(float freq, float sampleRate)
{
    float maxHarm = 0.5f * sampleRate / (freq > 1.0f ? freq : 1.0f);
    int level = 0;
    float h = 1024.0f;
    while (h > maxHarm && level < WT_LEVELS - 1) { h *= 0.5f; ++level; }
    return level;
}

// ---- factory banks ----
int numFactoryWavetables();
const char* factoryWavetableName(int index);
std::shared_ptr<WavetableSet> makeFactoryWavetable(int index);
int findFactoryWavetable(const std::string& name);   // -1 if unknown

// ---- single-frame generators (used by banks and the editor) ----
namespace wtgen {
    void sine(float* out);
    void triangle(float* out);
    void saw(float* out);
    void square(float* out);
    void pulse(float* out, float width);
    void additive(float* out, const float* amps, int numHarmonics);   // amps[0] = fundamental
    void noise(float* out, unsigned seed);
    void smooth(float* out, int passes);
    void normalize(float* out);
}

// In-place radix-2 FFT, size must be a power of two (exposed for the analyzer).
void fftComplex(std::vector<float>& re, std::vector<float>& im, bool inverse);
