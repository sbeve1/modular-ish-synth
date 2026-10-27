#include "engine/Wavetable.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

static constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------------
// FFT
// ---------------------------------------------------------------------
void fftComplex(std::vector<float>& re, std::vector<float>& im, bool inverse)
{
    const size_t n = re.size();

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2.0 * kPi / (double)len * (inverse ? 1.0 : -1.0);
        double wr = std::cos(ang), wi = std::sin(ang);
        for (size_t i = 0; i < n; i += len) {
            double cr = 1.0, ci = 0.0;
            for (size_t k = 0; k < len / 2; ++k) {
                size_t a = i + k, b = i + k + len / 2;
                double tr = re[b] * cr - im[b] * ci;
                double ti = re[b] * ci + im[b] * cr;
                re[b] = (float)(re[a] - tr); im[b] = (float)(im[a] - ti);
                re[a] = (float)(re[a] + tr); im[a] = (float)(im[a] + ti);
                double ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }

    if (inverse)
        for (size_t i = 0; i < n; ++i) { re[i] /= (float)n; im[i] /= (float)n; }
}

// ---------------------------------------------------------------------
// Build
// ---------------------------------------------------------------------
void WavetableSet::build()
{
    mips.assign((size_t)WT_LEVELS * WT_FRAMES * (WT_SIZE + 1), 0.0f);

    std::vector<float> specRe(WT_SIZE), specIm(WT_SIZE), re(WT_SIZE), im(WT_SIZE);

    for (int f = 0; f < WT_FRAMES; ++f) {
        float* src = frame(f);

        for (int i = 0; i < WT_SIZE; ++i) { specRe[i] = src[i]; specIm[i] = 0.0f; }
        fftComplex(specRe, specIm, false);
        specRe[0] = specIm[0] = 0.0f;                       // DC
        specRe[WT_SIZE / 2] = specIm[WT_SIZE / 2] = 0.0f;   // Nyquist

        // keep the source consistent with what plays (no DC)
        {
            re = specRe; im = specIm;
            fftComplex(re, im, true);
            float peak = 0.0f;
            for (float v : re) peak = std::max(peak, std::fabs(v));
            float g = peak > 1e-9f ? 1.0f / peak : 0.0f;
            for (int i = 0; i < WT_SIZE; ++i) src[i] = re[i] * g;
            for (int i = 0; i < WT_SIZE; ++i) { specRe[i] *= g; specIm[i] *= g; }
        }

        for (int level = 0; level < WT_LEVELS; ++level) {
            int maxHarm = std::min(1024 >> level, WT_SIZE / 2 - 1);
            for (int i = 0; i < WT_SIZE; ++i) {
                int h = i <= WT_SIZE / 2 ? i : WT_SIZE - i;
                bool keep = h >= 1 && h <= maxHarm;
                re[i] = keep ? specRe[i] : 0.0f;
                im[i] = keep ? specIm[i] : 0.0f;
            }
            fftComplex(re, im, true);

            float* dst = &mips[((size_t)level * WT_FRAMES + f) * (WT_SIZE + 1)];
            for (int i = 0; i < WT_SIZE; ++i) dst[i] = re[i];
            dst[WT_SIZE] = dst[0];
        }
    }
}

// ---------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------
namespace wtgen {

static inline double ph(int i) { return (double)i / WT_SIZE; }

void sine(float* o)     { for (int i = 0; i < WT_SIZE; ++i) o[i] = (float)std::sin(2 * kPi * ph(i)); }
void triangle(float* o) { for (int i = 0; i < WT_SIZE; ++i) { double p = ph(i); o[i] = (float)(p < 0.25 ? 4 * p : p < 0.75 ? 2 - 4 * p : 4 * p - 4); } }
void saw(float* o)      { for (int i = 0; i < WT_SIZE; ++i) o[i] = (float)(1.0 - 2.0 * ph(i)); }
void square(float* o)   { pulse(o, 0.5f); }
void pulse(float* o, float w) { for (int i = 0; i < WT_SIZE; ++i) o[i] = ph(i) < w ? 1.0f : -1.0f; }

void additive(float* o, const float* amps, int n)
{
    std::fill(o, o + WT_SIZE, 0.0f);
    for (int h = 0; h < n; ++h) {
        if (amps[h] == 0.0f) continue;
        for (int i = 0; i < WT_SIZE; ++i)
            o[i] += amps[h] * (float)std::sin(2 * kPi * (h + 1) * ph(i));
    }
}

void noise(float* o, unsigned seed)
{
    uint32_t s = seed * 2654435761u + 1u;
    for (int i = 0; i < WT_SIZE; ++i) {
        s = s * 1664525u + 1013904223u;
        o[i] = ((s >> 8) & 0xFFFF) / 32767.5f - 1.0f;
    }
}

void smooth(float* o, int passes)
{
    std::vector<float> t(WT_SIZE);
    for (int p = 0; p < passes; ++p) {
        for (int i = 0; i < WT_SIZE; ++i)
            t[i] = 0.25f * o[(i + WT_SIZE - 1) % WT_SIZE] + 0.5f * o[i] + 0.25f * o[(i + 1) % WT_SIZE];
        std::copy(t.begin(), t.end(), o);
    }
}

void normalize(float* o)
{
    float peak = 0.0f;
    for (int i = 0; i < WT_SIZE; ++i) peak = std::max(peak, std::fabs(o[i]));
    if (peak > 1e-9f) for (int i = 0; i < WT_SIZE; ++i) o[i] /= peak;
}

} // namespace wtgen

// ---------------------------------------------------------------------
// Factory banks
// ---------------------------------------------------------------------
static const char* const kFactoryNames[] = {
    "Basic Shapes", "Analog Sweep", "Harmonic Series", "Vocal", "Sync Sweep",
    "Pulse Width", "FM Bright", "Wavefold", "Spectra",
};
constexpr int kNumFactory = sizeof(kFactoryNames) / sizeof(kFactoryNames[0]);

int numFactoryWavetables() { return kNumFactory; }
const char* factoryWavetableName(int i) { return (i >= 0 && i < kNumFactory) ? kFactoryNames[i] : "Custom"; }

int findFactoryWavetable(const std::string& name)
{
    for (int i = 0; i < kNumFactory; ++i)
        if (name == kFactoryNames[i]) return i;
    return -1;
}

static void lowpassedSaw(float* o, float cutoffHarm, float resonance)
{
    float amps[512];
    for (int h = 1; h <= 512; ++h) {
        float x = h / cutoffHarm;
        float lp = 1.0f / std::sqrt(1.0f + std::pow(x, 8.0f));          // 4-pole-ish roll-off
        float peak = resonance * std::exp(-std::pow((h - cutoffHarm) / (0.15f * cutoffHarm + 1.0f), 2.0f));
        amps[h - 1] = (1.0f / h) * (lp + peak);
    }
    wtgen::additive(o, amps, 512);
}

static void vowel(float* o, const float f[3])
{
    const float f0 = 110.0f;
    float amps[96];
    for (int h = 1; h <= 96; ++h) {
        float freq = h * f0, a = 0.0f;
        for (int k = 0; k < 3; ++k) {
            float bw = 60.0f + 40.0f * k;
            a += (k == 0 ? 1.0f : k == 1 ? 0.6f : 0.3f) * std::exp(-0.5f * std::pow((freq - f[k]) / bw, 2.0f));
        }
        amps[h - 1] = a + 0.02f / h;
    }
    wtgen::additive(o, amps, 96);
}

std::shared_ptr<WavetableSet> makeFactoryWavetable(int index)
{
    auto wt = std::make_shared<WavetableSet>();
    wt->name = factoryWavetableName(index);
    using namespace wtgen;

    switch (index) {
    default:
    case 0: // Basic Shapes
        sine(wt->frame(0));
        triangle(wt->frame(1));
        saw(wt->frame(2)); smooth(wt->frame(2), 40);
        saw(wt->frame(3));
        square(wt->frame(4));
        pulse(wt->frame(5), 0.3f);
        pulse(wt->frame(6), 0.15f);
        pulse(wt->frame(7), 0.06f);
        break;

    case 1: { // Analog Sweep: resonant low-pass opening on a saw
        const float cut[WT_FRAMES] = { 1.5f, 2.5f, 4, 7, 12, 22, 45, 120 };
        for (int f = 0; f < WT_FRAMES; ++f) lowpassedSaw(wt->frame(f), cut[f], 2.5f);
        break;
    }

    case 2: { // Harmonic Series: organ -> buzz
        const int count[WT_FRAMES] = { 1, 2, 3, 5, 8, 13, 21, 40 };
        for (int f = 0; f < WT_FRAMES; ++f) {
            float amps[40] = {};
            for (int h = 0; h < count[f]; ++h) amps[h] = 1.0f / std::sqrt((float)h + 1.0f);
            additive(wt->frame(f), amps, 40);
        }
        break;
    }

    case 3: { // Vocal: A E I O U with blends
        const float A[3] = { 800, 1150, 2900 }, E[3] = { 400, 1700, 2600 }, I[3] = { 300, 2250, 3000 };
        const float O[3] = { 450, 800, 2830 },  U[3] = { 325, 700, 2530 };
        const float* seq[WT_FRAMES] = { A, nullptr, E, I, nullptr, O, U, nullptr };
        vowel(wt->frame(0), A); vowel(wt->frame(2), E); vowel(wt->frame(3), I);
        vowel(wt->frame(5), O); vowel(wt->frame(6), U);
        float AE[3], IO[3], UU[3];
        for (int k = 0; k < 3; ++k) { AE[k] = 0.5f * (A[k] + E[k]); IO[k] = 0.5f * (I[k] + O[k]); UU[k] = U[k] * 0.8f; }
        vowel(wt->frame(1), AE); vowel(wt->frame(4), IO); vowel(wt->frame(7), UU);
        (void)seq;
        break;
    }

    case 4: // Sync Sweep: hard-synced saw, slave ratio rising
        for (int f = 0; f < WT_FRAMES; ++f) {
            double ratio = 1.0 + f * 0.85;
            float* o = wt->frame(f);
            for (int i = 0; i < WT_SIZE; ++i) {
                double p = std::fmod((double)i / WT_SIZE * ratio, 1.0);
                o[i] = (float)(1.0 - 2.0 * p) * (1.0f - (float)i / WT_SIZE * 0.3f);
            }
        }
        break;

    case 5: // Pulse Width: 50% -> 4%
        for (int f = 0; f < WT_FRAMES; ++f) pulse(wt->frame(f), 0.5f - f * 0.065f);
        break;

    case 6: // FM Bright: sine modulated by 2x sine, rising index
        for (int f = 0; f < WT_FRAMES; ++f) {
            double index = f * 0.9;
            float* o = wt->frame(f);
            for (int i = 0; i < WT_SIZE; ++i) {
                double p = 2 * kPi * i / WT_SIZE;
                o[i] = (float)std::sin(p + index * std::sin(2.0 * p));
            }
        }
        break;

    case 7: // Wavefold: sine driven into a sine folder
        for (int f = 0; f < WT_FRAMES; ++f) {
            double gain = 1.0 + f * 1.1;
            float* o = wt->frame(f);
            for (int i = 0; i < WT_SIZE; ++i)
                o[i] = (float)std::sin(gain * 0.5 * kPi * std::sin(2 * kPi * i / WT_SIZE));
        }
        break;

    case 8: { // Spectra: seeded random harmonic spectra, darker -> brighter
        uint32_t s = 0x9E3779B9u;
        for (int f = 0; f < WT_FRAMES; ++f) {
            float amps[64];
            for (int h = 0; h < 64; ++h) {
                s = s * 1664525u + 1013904223u;
                float r = ((s >> 8) & 0xFFFF) / 65535.0f;
                float tilt = std::pow((float)(h + 1), -1.6f + f * 0.17f);
                amps[h] = (h == 0 ? 1.0f : r * r) * tilt;
            }
            additive(wt->frame(f), amps, 64);
        }
        break;
    }
    }

    wt->build();
    return wt;
}
