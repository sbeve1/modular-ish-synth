#include "engine/Effects.h"

#include <algorithm>
#include <cmath>

using namespace dsp;

// ---------------------------------------------------------------------
// Distortion
// ---------------------------------------------------------------------
void DistortionFX::process(float* l, float* r, int n, const Params& p)
{
    const float driveDb = p.drive * 36.0f;
    const float g = dbToGain(driveDb);
    const float makeup = 1.0f / std::sqrt(g);
    toneL.setCutoff(600.0f * std::pow(30.0f, p.tone), sr);   // 600 Hz .. 18 kHz
    toneR.a = toneL.a;
    dcL.setCutoff(15.0f, sr);
    dcR.a = dcL.a;

    const float bits = 16.0f - p.drive * 13.0f;
    const float q = std::exp2(bits - 1.0f);
    const float hold = 1.0f + p.drive * 15.0f;

    for (int i = 0; i < n; ++i) {
        float in[2] = { l[i], r[i] }, out[2];
        for (int ch = 0; ch < 2; ++ch) {
            float x = in[ch] * g, y;
            switch (p.mode) {
                case 0:  y = fastTanh(x); break;                                  // soft
                case 1:  y = std::clamp(x, -1.0f, 1.0f); break;                   // hard
                case 2:  y = x >= 0.0f ? fastTanh(x) : x / (1.0f + std::fabs(x)) * 0.8f; break;  // tube (asym)
                case 3:  y = std::sin(x * 0.5f * PI); break;                      // fold
                default: y = in[ch]; break;                                       // crush (below)
            }
            if (p.mode == 4) {
                if (ch == 0) {
                    crushCount += 1.0f;
                    if (crushCount >= hold) crushCount -= hold;
                }
                if (crushCount < 1.0f) crushHold[ch] = std::round(in[ch] * q) / q;
                y = crushHold[ch];
            } else {
                y *= std::max(makeup, 0.25f) * 1.6f;
            }
            y = ch == 0 ? toneL.lp(y) : toneR.lp(y);
            y = ch == 0 ? dcL.hp(y) : dcR.hp(y);
            out[ch] = y;
        }
        l[i] = lerp(in[0], out[0], p.mix);
        r[i] = lerp(in[1], out[1], p.mix);
    }
}

// ---------------------------------------------------------------------
// Chorus
// ---------------------------------------------------------------------
void ChorusFX::prepare(float sampleRate)
{
    sr = sampleRate;
    dl[0].allocate((int)(0.05f * sr));
    dl[1].allocate((int)(0.05f * sr));
}

void ChorusFX::process(float* l, float* r, int n, const Params& p)
{
    // modulation is computed every MOD_STEP samples and interpolated in between
    constexpr int MOD_STEP = 16;
    const float base = 0.012f * sr, depth = (0.0005f + p.depth * 0.006f) * sr;
    const float inc = p.rate / sr;
    for (int s0 = 0; s0 < n; s0 += MOD_STEP) {
        int len = std::min(MOD_STEP, n - s0);
        float ph1 = phase + inc * len;
        float dA[2] = { base + depth * fastSin(TWO_PI * phase), base + depth * fastSin(TWO_PI * (phase + 0.25f)) };
        float dB[2] = { base + depth * fastSin(TWO_PI * ph1),   base + depth * fastSin(TWO_PI * (ph1 + 0.25f)) };
        phase = ph1 - std::floor(ph1);
        for (int k = 0; k < len; ++k) {
            int i = s0 + k;
            float t = (k + 1) / (float)len;
            float in[2] = { l[i], r[i] };
            float wet[2];
            for (int ch = 0; ch < 2; ++ch) {
                wet[ch] = dl[ch].tap(dA[ch] + (dB[ch] - dA[ch]) * t);
                dl[ch].push(in[ch] + fb[ch] * p.feedback);
                fb[ch] = killDenormal(wet[ch]);
            }
            l[i] = lerp(in[0], wet[0], p.mix);
            r[i] = lerp(in[1], wet[1], p.mix);
        }
    }
}

// ---------------------------------------------------------------------
// Delay
// ---------------------------------------------------------------------
void DelayFX::prepare(float sampleRate)
{
    sr = sampleRate;
    dl[0].allocate((int)(4.1f * sr));
    dl[1].allocate((int)(4.1f * sr));
    time.setTime(0.08f, sr);
    time.snap(0.375f * sr);
}

void DelayFX::process(float* l, float* r, int n, const Params& p)
{
    float target = std::clamp(p.timeSec, 0.001f, 4.0f) * sr;
    float cutoff = 800.0f * std::pow(22.0f, p.tone);   // 800 Hz .. 17.6 kHz
    for (int ch = 0; ch < 2; ++ch) { lp[ch].setCutoff(cutoff, sr); hp[ch].setCutoff(80.0f, sr); }

    for (int i = 0; i < n; ++i) {
        float d = time.next(target);
        float tl = dl[0].tap(d), tr = dl[1].tap(d);
        float fl = hp[0].hp(lp[0].lp(tl)) * p.feedback;
        float fr = hp[1].hp(lp[1].lp(tr)) * p.feedback;

        if (p.pingPong) {
            dl[0].push(0.5f * (l[i] + r[i]) + fr);
            dl[1].push(fl);
        } else {
            dl[0].push(l[i] + fl);
            dl[1].push(r[i] + fr);
        }
        l[i] += tl * p.mix;
        r[i] += tr * p.mix;
    }
}

// ---------------------------------------------------------------------
// Reverb
// ---------------------------------------------------------------------
static const float kLineMs[ReverbFX::N] = { 37.1f, 43.7f, 51.3f, 59.9f, 67.3f, 73.1f, 83.9f, 97.7f };
static const float kDiffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };

void ReverbFX::prepare(float sampleRate)
{
    sr = sampleRate;
    pre.allocate((int)(0.25f * sr));
    for (int i = 0; i < 4; ++i) diff[i].allocate((int)(kDiffMs[i] * 0.001f * sr * 1.6f) + 8);
    for (int i = 0; i < N; ++i) line[i].allocate((int)(kLineMs[i] * 0.001f * sr * 1.6f) + 64);
    reset();
}

void ReverbFX::reset()
{
    pre.clear();
    for (auto& d : diff) d.clear();
    for (auto& d : line) d.clear();
    for (float& s : lpState) s = 0.0f;
}

static inline void hadamard8(float* x)
{
    for (int len = 1; len < 8; len <<= 1)
        for (int i = 0; i < 8; i += len << 1)
            for (int j = i; j < i + len; ++j) {
                float a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
    const float s = 0.35355339f;   // 1/sqrt(8)
    for (int i = 0; i < 8; ++i) x[i] *= s;
}

void ReverbFX::process(float* l, float* r, int n, const Params& p)
{
    const float scale = 0.4f + p.size * 1.2f;   // 0.4 .. 1.6
    float len[N], fbGain[N];
    for (int i = 0; i < N; ++i) {
        len[i] = kLineMs[i] * 0.001f * sr * scale;
        fbGain[i] = std::pow(10.0f, -3.0f * len[i] / (std::max(p.decay, 0.05f) * sr));
    }
    const float dampCoef = std::exp(-TWO_PI * (18000.0f * std::pow(0.06f, p.damping)) / sr);
    const int preSamples = std::clamp((int)(p.preDelayMs * 0.001f * sr), 0, pre.mask);
    float diffLen[4];
    for (int i = 0; i < 4; ++i) diffLen[i] = kDiffMs[i] * 0.001f * sr * (0.6f + 0.4f * scale);
    const float modInc = 0.37f / sr;

    for (int s = 0; s < n; ++s) {
        float in = 0.5f * (l[s] + r[s]);
        pre.push(in);
        float x = pre.tapInt(preSamples);

        for (int i = 0; i < 4; ++i) {   // series allpass diffusion
            float d = diff[i].tapInt((int)diffLen[i]);
            float v = x + 0.62f * d;
            diff[i].push(v);
            x = d - 0.62f * v;
        }

        modPhase += modInc;
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        float mod = fastSin(TWO_PI * modPhase) * 6.0f;

        float o[N];
        for (int i = 0; i < N; ++i) {
            float d = len[i] + ((i & 1) ? mod : -mod) * (i < 4 ? 1.0f : 0.5f);
            float v = line[i].tap(d);
            lpState[i] = v + dampCoef * (lpState[i] - v);
            o[i] = lpState[i];
        }

        float outL = o[0] + o[2] + o[4] + o[6];
        float outR = o[1] + o[3] + o[5] + o[7];

        float m[N];
        for (int i = 0; i < N; ++i) m[i] = o[i];
        hadamard8(m);
        for (int i = 0; i < N; ++i)
            line[i].push(killDenormal(m[i] * fbGain[i] + ((i & 1) ? x : -x) * 0.35f));

        float mid = 0.5f * (outL + outR), side = 0.5f * (outL - outR) * p.width;
        l[s] += (mid + side) * 0.5f * p.mix;
        r[s] += (mid - side) * 0.5f * p.mix;
    }
}

// ---------------------------------------------------------------------
// Biquad / EQ
// ---------------------------------------------------------------------
static void setCoefs(Biquad& q, float b0, float b1, float b2, float a0, float a1, float a2)
{
    q.b0 = b0 / a0; q.b1 = b1 / a0; q.b2 = b2 / a0; q.a1 = a1 / a0; q.a2 = a2 / a0;
}

void Biquad::lowShelf(float f, float gainDb, float sr)
{
    float A = std::pow(10.0f, gainDb / 40.0f), w = TWO_PI * std::min(f, sr * 0.45f) / sr;
    float c = std::cos(w), s = std::sin(w), sa = 2.0f * std::sqrt(A) * (s / 2.0f * std::sqrt(2.0f));
    setCoefs(*this, A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
             (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}

void Biquad::highShelf(float f, float gainDb, float sr)
{
    float A = std::pow(10.0f, gainDb / 40.0f), w = TWO_PI * std::min(f, sr * 0.45f) / sr;
    float c = std::cos(w), s = std::sin(w), sa = 2.0f * std::sqrt(A) * (s / 2.0f * std::sqrt(2.0f));
    setCoefs(*this, A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}

void Biquad::peak(float f, float q, float gainDb, float sr)
{
    float A = std::pow(10.0f, gainDb / 40.0f), w = TWO_PI * std::min(f, sr * 0.45f) / sr;
    float c = std::cos(w), alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));
    setCoefs(*this, 1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
}

float Biquad::magnitudeDb(float f, float sr) const
{
    float w = TWO_PI * f / sr;
    float c1 = std::cos(w), s1 = std::sin(w), c2 = std::cos(2 * w), s2 = std::sin(2 * w);
    float nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
    float dr = 1 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
    float mag = std::sqrt((nr * nr + ni * ni) / std::max(dr * dr + di * di, 1e-12f));
    return 20.0f * std::log10(std::max(mag, 1e-6f));
}

void EqFX::process(float* l, float* r, int n, const Params& p)
{
    band[0].lowShelf(p.lowHz, p.lowDb, sr);
    band[1].peak(p.midHz, p.midQ, p.midDb, sr);
    band[2].highShelf(p.highHz, p.highDb, sr);
    for (int i = 0; i < n; ++i)
        for (auto& b : band) { l[i] = b.tick(0, l[i]); r[i] = b.tick(1, r[i]); }
}

// ---------------------------------------------------------------------
// Compressor
// ---------------------------------------------------------------------
void CompressorFX::process(float* l, float* r, int n, const Params& p)
{
    const float atk = std::exp(-1.0f / (std::max(p.attackMs, 0.05f) * 0.001f * sr));
    const float rel = std::exp(-1.0f / (std::max(p.releaseMs, 1.0f) * 0.001f * sr));
    const float knee = 6.0f, slope = 1.0f - 1.0f / std::max(p.ratio, 1.0f);
    const float makeup = dbToGain(p.makeupDb);
    float maxGr = 0.0f;
    for (int i = 0; i < n; ++i) {
        float peak = std::max(std::fabs(l[i]), std::fabs(r[i]));
        float inDb = fastGainToDb(std::max(peak, 1e-6f));
        envDb = inDb > envDb ? inDb + atk * (envDb - inDb) : inDb + rel * (envDb - inDb);

        float over = envDb - p.threshDb, gr;
        if (over <= -knee * 0.5f)     gr = 0.0f;
        else if (over >= knee * 0.5f) gr = over * slope;
        else { float x = over + knee * 0.5f; gr = slope * x * x / (2.0f * knee); }
        maxGr = std::max(maxGr, gr);

        float g = fastDbToGain(-gr) * makeup;
        l[i] = lerp(l[i], l[i] * g, p.mix);
        r[i] = lerp(r[i], r[i] * g, p.mix);
    }
    grDb = maxGr;
}

// ---------------------------------------------------------------------
// Phaser: sweeping first-order all-passes; notches appear where the
// phase-shifted signal cancels the dry one.
// ---------------------------------------------------------------------
void PhaserFX::process(float* l, float* r, int n, const Params& p)
{
    // all-pass coefficients follow the LFO at control rate and are ramped
    // linearly across each MOD_STEP block
    constexpr int MOD_STEP = 16;
    const float inc = p.rate / sr;
    auto coefAt = [&](float ph, int ch) {
        float lfo = 0.5f + 0.5f * fastSin(TWO_PI * (ph + ch * 0.25f));
        float fc = 200.0f * fastExp2(lfo * p.depth * 4.6438562f);   // 200 Hz .. 5 kHz (25 = 2^4.64)
        float t = std::tan(PI * std::min(fc, sr * 0.45f) / sr);
        return (t - 1.0f) / (t + 1.0f);
    };
    if (!coefValid) { coef[0] = coefAt(phase, 0); coef[1] = coefAt(phase, 1); coefValid = true; }

    for (int s0 = 0; s0 < n; s0 += MOD_STEP) {
        int len = std::min(MOD_STEP, n - s0);
        phase += inc * len;
        phase -= std::floor(phase);
        float a0[2] = { coef[0], coef[1] };
        float a1[2] = { coefAt(phase, 0), coefAt(phase, 1) };
        float da[2] = { (a1[0] - a0[0]) / len, (a1[1] - a0[1]) / len };
        coef[0] = a1[0];
        coef[1] = a1[1];
        for (int k = 0; k < len; ++k) {
            int i = s0 + k;
            float in[2] = { l[i], r[i] };
            for (int ch = 0; ch < 2; ++ch) {
                float a = a0[ch] + da[ch] * (k + 1);
                float x = in[ch] + fb[ch] * p.feedback;
                for (int s = 0; s < STAGES; ++s) {
                    float y = a * x + ap[ch][s];
                    ap[ch][s] = x - a * y;
                    x = y;
                }
                fb[ch] = x;
                (ch == 0 ? l[i] : r[i]) = lerp(in[ch], (in[ch] + x) * 0.6f, p.mix);
            }
        }
    }
}

// ---------------------------------------------------------------------
// Flanger: short modulated delay mixed with the dry signal
// ---------------------------------------------------------------------
void FlangerFX::prepare(float sampleRate)
{
    sr = sampleRate;
    dl[0].allocate((int)(0.02f * sr));
    dl[1].allocate((int)(0.02f * sr));
}

void FlangerFX::process(float* l, float* r, int n, const Params& p)
{
    constexpr int MOD_STEP = 16;
    const float inc = p.rate / sr;
    const float base = 0.0006f * sr, depth = (0.0004f + p.depth * 0.0065f) * sr;
    for (int s0 = 0; s0 < n; s0 += MOD_STEP) {
        int len = std::min(MOD_STEP, n - s0);
        float ph1 = phase + inc * len;
        float dA[2], dB[2];
        for (int ch = 0; ch < 2; ++ch) {
            dA[ch] = base + depth * (0.5f + 0.5f * fastSin(TWO_PI * (phase + ch * 0.25f)));
            dB[ch] = base + depth * (0.5f + 0.5f * fastSin(TWO_PI * (ph1 + ch * 0.25f)));
        }
        phase = ph1 - std::floor(ph1);
        for (int k = 0; k < len; ++k) {
            int i = s0 + k;
            float t = (k + 1) / (float)len;
            float in[2] = { l[i], r[i] };
            for (int ch = 0; ch < 2; ++ch) {
                float d = killDenormal(dl[ch].tap(dA[ch] + (dB[ch] - dA[ch]) * t));
                dl[ch].push(in[ch] + d * p.feedback);
                (ch == 0 ? l[i] : r[i]) = lerp(in[ch], (in[ch] + d) * 0.7f, p.mix);
            }
        }
    }
}

// ---------------------------------------------------------------------
// Limiter: peak follower, instant attack, ~120 ms release, plus a soft clip
// ---------------------------------------------------------------------
void Limiter::process(float* l, float* r, int n, float ceiling)
{
    const float rel = std::exp(-1.0f / (0.12f * sr));
    for (int i = 0; i < n; ++i) {
        float peak = std::max(std::fabs(l[i]), std::fabs(r[i]));
        env = peak > env ? peak : peak + rel * (env - peak);
        float target = env > ceiling ? ceiling / env : 1.0f;
        gain = target < gain ? target : target + rel * (gain - target);
        l[i] = fastTanh(l[i] * gain / ceiling) * ceiling;
        r[i] = fastTanh(r[i] * gain / ceiling) * ceiling;
    }
}
