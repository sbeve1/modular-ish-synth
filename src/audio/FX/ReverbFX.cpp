#include "ReverbFX.h"
#include "engine/ModTargetID.h"
#include <cstring> // memset
#include <algorithm>
#include <random>

// ----------------------------------------------------------------------------
// Constants / small helpers
// ----------------------------------------------------------------------------
static inline float clampf_local(float v, float a, float b) { return std::max(a, std::min(b, v)); }
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
static inline float smoothstep(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static inline float logMap(float t, float minHz, float maxHz)
{
    float logMin = std::log(minHz);
    float logMax = std::log(maxHz);
    return std::exp(logMin + t * (logMax - logMin));
}
static inline float serumHighCutNormToHz(float t)
{
    // t: 0..1
    t = std::clamp(t, 0.0f, 1.0f);

    constexpr float minHz = 1000.0f;   // IMPORTANT: higher than LO’s low reach
    constexpr float maxHz = 20000.0f;

    constexpr float knee = 0.40f;

    float shaped;
    if (t < knee)
    {
        // completely open
        shaped = 1.0f;
    }
    else
    {
        float x = (t - knee) / (1.0f - knee);
        // different curve, later toe-in
        shaped = 1.0f - smoothstep(x);
    }

    return logMap(shaped, minHz, maxHz);
}

static inline float serumLowCutNormToHz(float t)
{
    // t: 0..1
    t = std::clamp(t, 0.0f, 1.0f);

    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;

    constexpr float knee = 0.30f;

    float shaped;
    if (t < knee)
    {
        // flat / barely affecting
        shaped = 0.0f;
    }
    else
    {
        float x = (t - knee) / (1.0f - knee);
        // curved toe → straight-ish run
        shaped = smoothstep(x);
    }

    return logMap(shaped, minHz, maxHz);
}


// ----------------------------------------------------------------------------
// ReverbFX implementation
// ----------------------------------------------------------------------------

ReverbFX::ReverbFX(float sampleRate_)
    : sr(sampleRate_)
{
    // reasonable buffer lengths for pre-delay and delay lines
    size_t maxDelay = (size_t)std::ceil(sr * 3.0f) + 8; // 3 seconds
    delayL.assign(maxDelay, 0.0f);
    delayR.assign(maxDelay, 0.0f);
    idxL = idxR = 0;

    // init per-mode state containers (sizes chosen empirically)
    // Plate: combs + allpasses
    plateCombs.resize(6);
    plateCombIndex.assign(6, 0);
    for (int i = 0; i < 6; ++i) {
        plateCombs[i].resize((size_t)std::max(1, (int)(sr * (0.03f + i * 0.011f))));
        std::fill(plateCombs[i].begin(), plateCombs[i].end(), 0.0f);
    }
    plateAllpasses.resize(3);
    plateAllpassIndex.assign(3, 0);
    for (int i = 0; i < 3; ++i) {
        plateAllpasses[i].resize((size_t)std::max(1, (int)(sr * (0.005f + i * 0.003f))));
        std::fill(plateAllpasses[i].begin(), plateAllpasses[i].end(), 0.0f);
    }
    plateDampL.reset();
    plateDampR.reset();
    plateAllpassGain = 0.5f;

    // Hall: reuse some plate buffers but add modulation LFO and longer tail combs
    hallCombs.resize(8);
    hallCombIndex.assign(8, 0);
    for (int i = 0; i < 8; ++i) {
        hallCombs[i].resize((size_t)std::max(1, (int)(sr * (0.05f + i * 0.02f))));
        std::fill(hallCombs[i].begin(), hallCombs[i].end(), 0.0f);
    }
    hallAllpass.resize(4);
    hallAllpassIndex.assign(4, 0);
    for (int i = 0; i < 4; ++i) {
        hallAllpass[i].resize((size_t)std::max(1, (int)(sr * (0.003f + i * 0.004f))));
        std::fill(hallAllpass[i].begin(), hallAllpass[i].end(), 0.0f);
    }
    hallDampL.reset();
    hallDampR.reset();

    // Vintage: early reflections taps + diffusers + chorus
    vintageERL.resize(8); vintageERR.resize(8);
    vintageERTaps = {0.012f, 0.025f, 0.038f, 0.053f, 0.078f, 0.103f, 0.141f, 0.185f};
    // memory for ER tapped delays (will be offsets to main circular buffer)
    vintageDiffL.resize(4); vintageDiffR.resize(4);
    for (auto &v : vintageDiffL) v.resize((size_t)std::max(1, (int)(sr * 0.02f)));
    for (auto &v : vintageDiffR) v.resize((size_t)std::max(1, (int)(sr * 0.02f)));
    vintageDiffIndex.assign(4, 0);
    vintageChorusPhase = 0.0f;

    // Nitrous: 4x4 FDN
    fdnDelays.resize(4);
    fdnIndex.assign(4, 0);
    fdnBufLens.assign(4, 0);
    // default FDN lengths (co-prime-ish)
    fdnBufLens[0] = (size_t)std::round(sr * 0.0297f);
    fdnBufLens[1] = (size_t)std::round(sr * 0.0371f);
    fdnBufLens[2] = (size_t)std::round(sr * 0.0413f);
    fdnBufLens[3] = (size_t)std::round(sr * 0.0437f);
    for (int i = 0; i < 4; ++i) {
        fdnDelays[i].resize(fdnBufLens[i]);
        std::fill(fdnDelays[i].begin(), fdnDelays[i].end(), 0.0f);
    }
    // default FDN matrices (5 preset geometries)
    initFDNMatrixPresets();

    // Basin: simple feedback tank with lowpass in loop and a small chorus
    basinDelay.resize((size_t)std::round(sr * 0.035f)); // 35ms
    std::fill(basinDelay.begin(), basinDelay.end(), 0.0f);
    basinIndex = 0;
    basinLowpassL.reset(); basinLowpassR.reset();
    // generic DSP init
    resetState();
}

ReverbFX::~ReverbFX()
{
    // nothing special
}

// ----------------------------------------------------------------------------
// Utility: reset internal state (call also on sample rate change)
void ReverbFX::resetState()
{
    // clear all buffers & integrators
    std::fill(delayL.begin(), delayL.end(), 0.0f);
    std::fill(delayR.begin(), delayR.end(), 0.0f);
    idxL = idxR = 0;

    for (auto &buf : plateCombs) std::fill(buf.begin(), buf.end(), 0.0f);
    for (auto &buf : plateAllpasses) std::fill(buf.begin(), buf.end(), 0.0f);
    for (auto &b : plateCombIndex) b = 0;
    for (auto &b : plateAllpassIndex) b = 0;

    for (auto &buf : hallCombs) std::fill(buf.begin(), buf.end(), 0.0f);
    for (auto &buf : hallAllpass) std::fill(buf.begin(), buf.end(), 0.0f);
    for (auto &b : hallCombIndex) b = 0;
    for (auto &b : hallAllpassIndex) b = 0;

    for (auto &v : vintageDiffL) std::fill(v.begin(), v.end(), 0.0f);
    for (auto &v : vintageDiffR) std::fill(v.begin(), v.end(), 0.0f);
    std::fill(vintageERL.begin(), vintageERL.end(), 0.0f);
    std::fill(vintageERR.begin(), vintageERR.end(), 0.0f);
    std::fill(vintageDiffIndex.begin(), vintageDiffIndex.end(), 0);

    for (auto &d : fdnDelays) std::fill(d.begin(), d.end(), 0.0f);
    std::fill(fdnIndex.begin(), fdnIndex.end(), 0);

    std::fill(basinDelay.begin(), basinDelay.end(), 0.0f);
    basinIndex = 0;

    plateDampL.reset(); plateDampR.reset();
    hallDampL.reset(); hallDampR.reset();
    basinLowpassL.reset(); basinLowpassR.reset();
}

// ----------------------------------------------------------------------------
// setControls: copy controls safely
// ----------------------------------------------------------------------------
void ReverbFX::setControls(const Controls& c)
{
    controls = c;
}

// ----------------------------------------------------------------------------
// process: top-level entrypoint
// ----------------------------------------------------------------------------
Sample ReverbFX::processWet(const Sample& in, float sampleRate)
{
    // update sample rate if needed
    if (sampleRate != sr) {
        sr = sampleRate;
        // resize buffers if necessary (conservative)
        size_t maxDelay = (size_t)std::ceil(sr * 3.0f) + 8;
        if (delayL.size() < maxDelay) {
            delayL.assign(maxDelay, 0.0f);
            delayR.assign(maxDelay, 0.0f);
        }
        // recompute internal delay lengths that depend on sr
        // (for simplicity we won't shrink lengths to avoid pointer re-index issues)
        resetState();
    }

    // quick bypass if global enable is false
    Controls c = controls;

    // process by mode
    Sample wet = processByMode(in);

    // Serum-ish ranges
    float loNorm = std::clamp(c.lowCut.base  + c.lowCut.mod,  0.0f, 1.0f);
    float hiNorm = std::clamp(c.highCut.base + c.highCut.mod, 0.0f, 1.0f);

    float lowCutHz  = serumLowCutNormToHz(loNorm);
    float highCutHz = serumHighCutNormToHz(hiNorm);

    hpL.setCutoff(lowCutHz, sr);
    hpR.setCutoff(lowCutHz, sr);

    lpL.setCutoff(highCutHz, sr);
    lpR.setCutoff(highCutHz, sr);



    // ---- LEFT ----
    float l = wet.left;
    float lpL_out = lpL.process(l);
    float hpL_lp  = hpL.process(l);
    wet.left = lpL_out - hpL_lp;

    // ---- RIGHT ----
    float r = wet.right;
    float lpR_out = lpR.process(r);
    float hpR_lp  = hpR.process(r);
    wet.right = lpR_out - hpR_lp;

    wet.left  = smallSat(wet.left);
    wet.right = smallSat(wet.right);

    return wet;
}

// ----------------------------------------------------------------------------
// processByMode: dispatch to specific mode implementations
// ----------------------------------------------------------------------------
Sample ReverbFX::processByMode(const Sample& in)
{
    Controls c = controls;

    // apply pre-delay to a circular pre-delay buffer and obtain delayed input sample
    // preDelay in seconds
    float preDelaySec = clampf_local(c.preDelay.base + c.preDelay.mod, 0.0f, 2.0f);
    size_t preDelaySamples = (size_t)std::round(preDelaySec * sr);
    // we use the main delay buffers as pre-delay storage (wrap-safe)
    size_t bufLen = delayL.size();
    // compute write position (current idxL is used as write pointer)
    // write incoming to buffer at current index
    delayL[idxL] = in.left;
    delayR[idxR] = in.right;
    // compute read position backward by preDelaySamples
    size_t readPosL = (idxL + bufLen - preDelaySamples) % bufLen;
    size_t readPosR = (idxR + bufLen - preDelaySamples) % bufLen;
    Sample pdIn{};
    pdIn.left  = delayL[readPosL];
    pdIn.right = delayR[readPosR];

    // advance write indices
    idxL = (idxL + 1) % bufLen;
    idxR = (idxR + 1) % bufLen;

    Sample wet{};
    switch (c.mode) {
        case Mode::PLATE:   wet = processPlate(pdIn); break;
        case Mode::HALL:    wet = processHall(pdIn); break;
        case Mode::VINTAGE: wet = processVintage(pdIn); break;
        case Mode::NITROUS: wet = processNitrous(pdIn); break;
        case Mode::BASIN:   wet = processBasin(pdIn); break;
        default:            wet = Sample{0.0f, 0.0f}; break;
    }

    return wet;
}

// ----------------------------------------------------------------------------
// Plate implementation (classic parallel combs + serial allpass diffusion)
// ----------------------------------------------------------------------------
Sample ReverbFX::processPlate(const Sample& in)
{
    Controls c = controls;

    // calculate parameters
    float size = clampf_local(c.size.base + c.size.mod, 0.01f, 2.5f);
    float damp = clampf_local(c.plateDamp.base + c.plateDamp.mod, 0.0f, 0.99f);
    float width = clampf_local(c.plateWidth.base + c.plateWidth.mod, 0.0f, 1.0f);

    // comb stage (parallel)
    float combOutL = 0.0f;
    float combOutR = 0.0f;
    float feedbackBase = clampf_local(0.94f + (size - 0.6f) * 0.2f, 0.6f, 0.995f);
    for (size_t i = 0; i < plateCombs.size(); ++i) {
        auto &buf = plateCombs[i];
        size_t len = buf.size();
        size_t &idx = plateCombIndex[i];

        // dynamic short variation per comb based on size
        size_t readIndex = (idx + len - (size_t)std::max(1u, (unsigned)((float)len * size * (0.3f + 0.12f * (int)i)))) % len;

        float inSum = in.left * (1.0f - width) + in.right * width; // feed stereo into combs with width
        float y = buf[readIndex];
        float fb = feedbackBase;
        float outSample = y;
        // damping inside feedback path (simple one-pole on feedback)
        float newVal = inSum + y * fb * (1.0f - damp);
        buf[idx] = newVal;
        idx = (idx + 1) % len;
        combOutL += outSample;
        combOutR += outSample;
    }

    // normalize comb sum
    combOutL *= (1.0f / (float)plateCombs.size());
    combOutR *= (1.0f / (float)plateCombs.size());

    // allpass diffusion chain (serial)
    float ap = combOutL;
    for (size_t i = 0; i < plateAllpasses.size(); ++i) {
        auto &buf = plateAllpasses[i];
        size_t len = buf.size();
        size_t &idx = plateAllpassIndex[i];
        size_t readIndex = (idx + len - 1) % len;
        float bufOut = buf[readIndex];
        float inVal = ap;
        float g = plateAllpassGain; // feedback coefficient
        float y = -g * inVal + bufOut;
        buf[idx] = inVal + g * y;
        idx = (idx + 1) % len;
        ap = y;
    }
    float tailL = ap;
    // feed some stereo width by mixing with combOutR
    tailL = (1.0f - width) * tailL + width * combOutR;

    // right channel do similar but rotate comb selection for slight stereo decorrelation
    float apR = combOutR;
    for (size_t i = 0; i < plateAllpasses.size(); ++i) {
        auto &buf = plateAllpasses[i];
        size_t len = buf.size();
        size_t &idx = plateAllpassIndex[i];
        size_t readIndex = (idx + len - 3) % len;
        float bufOut = buf[readIndex];
        float inVal = apR;
        float g = plateAllpassGain;
        float y = -g * inVal + bufOut;
        buf[idx] = inVal + g * y;
        idx = (idx + 1) % len;
        apR = y;
    }
    float tailR = apR;
    tailR = (1.0f - width) * tailR + width * combOutL;

    // apply damping (cheap lowpass in feedback was approximated above; add final smoothing)
    tailL = plateDampL.process(tailL * 0.9f);
    tailR = plateDampR.process(tailR * 0.9f);

    // level adjust
    float outGain = 0.8f;
    Sample out{};
    out.left  = tailL * outGain;
    out.right = tailR * outGain;
    return out;
}

// ----------------------------------------------------------------------------
// Hall implementation: longer tails + modulation (spin) applied to comb delays
// ----------------------------------------------------------------------------
Sample ReverbFX::processHall(const Sample& in)
{
    Controls c = controls;

    float size = clampf_local(c.size.base + c.size.mod, 0.1f, 4.0f);
    float decay = clampf_local(c.hallDecay.base + c.hallDecay.mod, 0.2f, 10.0f);
    float spinRate = clampf_local(c.hallSpinRate.base + c.hallSpinRate.mod, 0.0f, 2.0f);
    float spinDepth = clampf_local(c.hallSpinDepth.base + c.hallSpinDepth.mod, 0.0f, 0.7f);
    float damp = 0.4f; // fixed mild damping

    // drive LFO phase
    hallLfoPhase += (spinRate / sr) * 2.0f * float(M_PI);
    if (hallLfoPhase > 2.0f * float(M_PI)) hallLfoPhase -= 2.0f * float(M_PI);

    // comb network
    float accL = 0.0f, accR = 0.0f;
    float feedbackBase = clampf_local(0.92f + (size - 0.6f) * 0.15f + (decay - 2.5f) * 0.06f, 0.5f, 0.998f);

    for (size_t i = 0; i < hallCombs.size(); ++i) {
        auto &buf = hallCombs[i];
        size_t len = buf.size();
        size_t &idx = hallCombIndex[i];

        // apply spin modulation to read length slightly
        float mod = 1.0f + spinDepth * 0.0015f * std::sin(hallLfoPhase * (1.0f + 0.2f * (float)i));
        size_t readIndex = (idx + len - (size_t)std::max(1u, (unsigned)((float)len * 0.25f * size * mod))) % len;

        float inSum = in.left * 0.5f + in.right * 0.5f;
        float y = buf[readIndex];
        float outSample = y;
        float newVal = inSum + y * feedbackBase * (1.0f - damp);
        buf[idx] = newVal;
        idx = (idx + 1) % len;
        accL += outSample;
        accR += outSample;
    }

    accL *= (1.0f / (float)hallCombs.size());
    accR *= (1.0f / (float)hallCombs.size());

    // serial allpass diffusion
    float apL = accL, apR = accR;
    for (size_t i = 0; i < hallAllpass.size(); ++i) {
        auto &buf = hallAllpass[i];
        size_t len = buf.size();
        size_t &idx = hallAllpassIndex[i];
        size_t readIndex = (idx + len - 1) % len;
        float bufOut = buf[readIndex];
        float inValL = apL;
        float inValR = apR;
        float g = 0.7f - 0.06f * (float)i;
        // left
        float yL = -g * inValL + bufOut;
        buf[idx] = inValL + g * yL;
        // right (simple decorrelation)
        size_t readIndexR = (idx + len - 3) % len;
        float bufOutR = buf[readIndexR];
        float yR = -g * inValR + bufOutR;
        buf[idx] = inValR + g * yR;
        idx = (idx + 1) % len;
        apL = yL; apR = yR;
    }

    // damping and spread
    float width = clampf_local(c.plateWidth.base + c.plateWidth.mod, 0.0f, 1.0f);
    float tailL = hallDampL.process(apL);
    float tailR = hallDampR.process(apR);
    tailL = (1.0f - width) * tailL + width * tailR;
    tailR = (1.0f - width) * tailR + width * tailL;

    Sample out{};
    out.left  = tailL * 0.95f;
    out.right = tailR * 0.95f;
    return out;
}

// ----------------------------------------------------------------------------
// Vintage: early reflections + diffusion + chorus on tail
// ----------------------------------------------------------------------------
Sample ReverbFX::processVintage(const Sample& in)
{
    Controls c = controls;

    float size = clampf_local(c.size.base + c.size.mod, 0.05f, 2.0f);
    float decay = clampf_local(c.vintageDamp.base + c.vintageDamp.mod, 0.2f, 8.0f);
    float erSize = clampf_local(c.vintageERSize.base + c.vintageERSize.mod, 0.001f, 0.1f);
    float diffA = clampf_local(c.vintageDiffA.base + c.vintageDiffA.mod, 0.0f, 1.0f);
    float diffB = clampf_local(c.vintageDiffB.base + c.vintageDiffB.mod, 0.0f, 1.0f);
    float chorusRate = clampf_local(c.vintageChorus.rateBase + c.vintageChorus.rateMod, 0.0f, 5.0f);
    float chorusDepth = clampf_local(c.vintageChorus.depthBase + c.vintageChorus.depthMod, 0.0f, 0.05f);

    // early reflections: multiple taps
    Sample erOut{0.0f, 0.0f};
    for (size_t i = 0; i < vintageERTaps.size(); ++i) {
        size_t tapSamples = (size_t)std::round(vintageERTaps[i] * sr * (0.5f + size*0.7f));
        size_t readPos = (idxL + delayL.size() - tapSamples) % delayL.size();
        float valL = delayL[readPos];
        size_t readPosR = (idxR + delayR.size() - tapSamples - 3) % delayR.size();
        float valR = delayR[readPosR];
        // apply simple diffusion factor
        float gain = 0.25f / (1.0f + (float)i * 0.15f);
        erOut.left  += valL * gain;
        erOut.right += valR * gain;
    }

    // feed into a small diffusion chain (allpass-ish)
    float d = (erOut.left + erOut.right) * 0.5f;
    for (size_t i = 0; i < vintageDiffL.size(); ++i) {
        auto &bufL = vintageDiffL[i];
        auto &bufR = vintageDiffR[i];
        size_t &di = vintageDiffIndex[i];
        size_t len = bufL.size();
        size_t readIndex = (di + len - 1) % len;
        float bufOutL = bufL[readIndex];
        float bufOutR = bufR[readIndex];
        float g = 0.6f + 0.25f * (float)i;
        float yL = -g * d + bufOutL;
        bufL[di] = d + g * yL;
        float yR = -g * d + bufOutR;
        bufR[di] = d + g * yR;
        di = (di + 1) % len;
        d = 0.5f * (yL + yR);
    }

    // late tail approximation: reuse hall comb network lightly scaled by decay
    float tailL = 0.0f, tailR = 0.0f;
    float feedback = clampf_local(0.88f + (decay - 2.5f) * 0.06f + (size - 0.6f)*0.05f, 0.5f, 0.995f);
    for (size_t i = 0; i < plateCombs.size(); ++i) {
        auto &buf = plateCombs[i];
        size_t len = buf.size();
        size_t &pi = plateCombIndex[i];
        size_t readIndex = (pi + len - (size_t)std::max(1u, (unsigned)((float)len * 0.02f * size))) % len;
        float y = buf[readIndex];
        float inVal = d * 0.8f + (in.left + in.right) * 0.25f;
        buf[pi] = inVal + y * feedback;
        pi = (pi + 1) % len;
        tailL += y;
        tailR += y;
    }
    tailL *= (1.0f / (float)plateCombs.size());
    tailR *= (1.0f / (float)plateCombs.size());

    // vintage chorus on tail
    vintageChorusPhase += (chorusRate / sr) * 2.0f * float(M_PI);
    if (vintageChorusPhase > 2.0f * float(M_PI)) vintageChorusPhase -= 2.0f * float(M_PI);
    float mod = 1.0f + chorusDepth * std::sin(vintageChorusPhase);
    float vL = tailL * mod;
    float vR = tailR * (1.0f - 0.5f * chorusDepth * std::cos(vintageChorusPhase));

    // mix ER + tail
    Sample out{};
    out.left  = erOut.left * 0.6f + vL * 0.9f;
    out.right = erOut.right * 0.6f + vR * 0.9f;
    // mild damping
    out.left  = plateDampL.process(out.left);
    out.right = plateDampR.process(out.right);
    return out;
}

// ----------------------------------------------------------------------------
// Nitrous: 4x4 FDN with selectable preset matrices
// ----------------------------------------------------------------------------
void ReverbFX::initFDNMatrixPresets()
{
    // Prepare a few orthogonal-ish matrices (normalized) for "space" types.
    // Each is 4x4. We'll store flattened row-major.
    fdnMatrices.clear();

    // Space - simple Hadamard-like
    fdnMatrices.push_back({
        0.5f,  0.5f,  0.5f,  0.5f,
        0.5f, -0.5f,  0.5f, -0.5f,
        0.5f,  0.5f, -0.5f, -0.5f,
        0.5f, -0.5f, -0.5f,  0.5f
    });

    // Marble - jittered orthonormal
    fdnMatrices.push_back({
        0.4f,  0.6f,  0.4f, -0.4f,
        -0.5f, 0.5f,  0.5f,  0.5f,
        0.6f, -0.4f,  0.6f, -0.4f,
        -0.3f, -0.3f, 0.7f,  0.6f
    });

    // Rectangle
    fdnMatrices.push_back({
        0.6f,  0.2f, -0.4f, 0.4f,
        -0.2f, 0.8f,  0.4f, 0.2f,
        0.4f, -0.5f,  0.6f, -0.2f,
        0.3f,  0.3f, -0.3f, 0.8f
    });

    // Hexagon
    fdnMatrices.push_back({
        0.3f, 0.7f,  0.1f, -0.5f,
        0.2f, -0.6f, 0.7f,  0.1f,
        -0.4f, 0.3f, 0.6f,  0.4f,
        0.6f, -0.2f, -0.4f, 0.5f
    });

    // Box
    fdnMatrices.push_back({
        0.9f, -0.1f, 0.2f,  0.0f,
        0.0f,  0.8f, -0.2f, 0.3f,
        0.2f,  0.1f,  0.7f, -0.4f,
        -0.1f, 0.3f,  0.4f,  0.6f
    });
}

// ----------------------------------------------------------------------------
Sample ReverbFX::processNitrous(const Sample& in)
{
    Controls c = controls;

    float size = clampf_local(c.size.base + c.size.mod, 0.02f, 1.8f);
    float diffusion = clampf_local(c.nitrousDiff.base + c.nitrousDiff.mod, 0.0f, 1.0f);
    float feedback = clampf_local(c.nitrousFeedback.base + c.nitrousFeedback.mod, -0.9f, 0.98f);
    int mode = clampf_local((float)c.nitrousModeIndex, 0.0f, (float)std::max(0,(int)fdnMatrices.size()-1));
    float chorusRate = clampf_local(c.nitrousChorus.rateBase + c.nitrousChorus.rateMod, 0.0f, 6.0f);
    float chorusDepth = clampf_local(c.nitrousChorus.depthBase + c.nitrousChorus.depthMod, 0.0f, 0.03f);

    // read outputs from FDN delays
    float inMix = 0.5f * (in.left + in.right);
    // read current indices and compute new values
    float outputs[4] = {0,0,0,0};
    for (int i = 0; i < 4; ++i) {
        size_t len = fdnDelays[i].size();
        size_t read = (fdnIndex[i] + len - 1) % len;
        outputs[i] = fdnDelays[i][read];
    }

    // apply feedback matrix
    const std::vector<float>& M = fdnMatrices[mode];
    float fed[4] = {0,0,0,0};
    for (int r = 0; r < 4; ++r) {
        float acc = 0.0f;
        for (int c2 = 0; c2 < 4; ++c2) {
            acc += M[r*4 + c2] * outputs[c2];
        }
        fed[r] = acc * feedback;
    }

    // write into each delay with diffusion + input injection
    for (int i = 0; i < 4; ++i) {
        float inVal = inMix * (0.2f + 0.8f * size); // input gain scales with size
        float diffed = fed[i] * (0.9f - 0.6f * diffusion);
        size_t writeIdx = fdnIndex[i];
        fdnDelays[i][writeIdx] = inVal + diffed;
        fdnIndex[i] = (fdnIndex[i] + 1) % fdnDelays[i].size();
    }

    // sum FDN outputs to stereo with simple decorrelation
    float sum = 0.0f;
    for (int i = 0; i < 4; ++i) sum += outputs[i];
    float outL = sum * 0.5f;
    float outR = (outputs[0] - outputs[1] + outputs[2] - outputs[3]) * 0.5f;

    // small chorus for shimmer
    nitrousChorusPhase += (chorusRate / sr) * 2.0f * float(M_PI);
    if (nitrousChorusPhase > 2.0f * float(M_PI)) nitrousChorusPhase -= 2.0f * float(M_PI);
    float mod = 1.0f + chorusDepth * std::sin(nitrousChorusPhase);
    outL *= mod;
    outR *= (1.0f - 0.3f * chorusDepth * std::cos(nitrousChorusPhase));

    // clamp & final shaping
    Sample out{};
    out.left  = clampf_local(outL, -1.0f, 1.0f) * 0.9f;
    out.right = clampf_local(outR, -1.0f, 1.0f) * 0.9f;
    return out;
}

// ----------------------------------------------------------------------------
// Basin: resonant tank / feedback delay with lowpass in loop + chorus
// ----------------------------------------------------------------------------
Sample ReverbFX::processBasin(const Sample& in)
{
    Controls c = controls;

    float feedback = clampf_local(c.basinFeedback.base + c.basinFeedback.mod, 0.0f, 0.98f);
    float chorusRate = clampf_local(c.basinChorus.rateBase + c.basinChorus.rateMod, 0.0f, 4.0f);
    float chorusDepth = clampf_local(c.basinChorus.depthBase + c.basinChorus.depthMod, 0.0f, 0.03f);

    // read current basin sample
    size_t len = basinDelay.size();
    size_t read = (basinIndex + len - 1) % len;
    float fedL = basinDelay[read];

    // loop: out = in + filtered feedback
    float fb = basinLowpassL.process(fedL) * feedback;
    float inMix = 0.5f * (in.left + in.right);
    float writeVal = inMix + fb;
    basinDelay[basinIndex] = writeVal;
    basinIndex = (basinIndex + 1) % basinDelay.size();

    // small chorus on output
    basinChorusPhase += (chorusRate / sr) * 2.0f * float(M_PI);
    if (basinChorusPhase > 2.0f * float(M_PI)) basinChorusPhase -= 2.0f * float(M_PI);
    float mod = 1.0f + chorusDepth * std::sin(basinChorusPhase);
    Sample out{};
    out.left  = writeVal * mod * 0.9f;
    out.right = writeVal * (1.0f - 0.2f * chorusDepth * std::cos(basinChorusPhase)) * 0.9f;
    return out;
}

void ReverbFX::getAvailableModTargets(std::vector<ModTargetID>& out) const
    {
        // shared
        out.push_back(ModTargetID::REVERB_SIZE);
        out.push_back(ModTargetID::REVERB_PREDELAY);
        out.push_back(ModTargetID::REVERB_LOW_CUT);
        out.push_back(ModTargetID::REVERB_HIGH_CUT);
        out.push_back(ModTargetID::FX_WET);

        switch (controls.mode) {
            case Mode::PLATE:
                out.push_back(ModTargetID::REVERB_PLATE_DAMP);
                out.push_back(ModTargetID::REVERB_PLATE_WIDTH);
                break;

            case Mode::HALL:
                out.push_back(ModTargetID::REVERB_HALL_DECAY);
                out.push_back(ModTargetID::REVERB_HALL_SPIN_RATE);
                out.push_back(ModTargetID::REVERB_HALL_SPIN_DEPTH);
                break;

            case Mode::VINTAGE:
                out.push_back(ModTargetID::REVERB_VINTAGE_ER_SIZE);
                out.push_back(ModTargetID::REVERB_VINTAGE_DAMP);
                out.push_back(ModTargetID::REVERB_VINTAGE_DIFF_A);
                out.push_back(ModTargetID::REVERB_VINTAGE_DIFF_B);
                out.push_back(ModTargetID::REVERB_VINTAGE_CHORUS_RATE);
                out.push_back(ModTargetID::REVERB_VINTAGE_CHORUS_DEPTH);
                break;

            case Mode::NITROUS:
                out.push_back(ModTargetID::REVERB_NITROUS_DIFFUSION);
                out.push_back(ModTargetID::REVERB_NITROUS_FEEDBACK);
                out.push_back(ModTargetID::REVERB_NITROUS_CHORUS_RATE);
                out.push_back(ModTargetID::REVERB_NITROUS_CHORUS_DEPTH);
                break;

            case Mode::BASIN:
                out.push_back(ModTargetID::REVERB_BASIN_FEEDBACK);
                out.push_back(ModTargetID::REVERB_BASIN_CHORUS_RATE);
                out.push_back(ModTargetID::REVERB_BASIN_CHORUS_DEPTH);
                break;

            default:
                break;
        }
    }

void ReverbFX::registerModTargets(ModMatrix& mod)
{
    // ---- FX-slot (already in base, but explicit is fine) ----
    mod.registerTarget(
        ModTargetID::FX_WET,
        &wetMod,
        wetBase,
        0.0f, 1.0f
    );

    // ---- Shared Reverb ----
    mod.registerTarget(
        ModTargetID::REVERB_SIZE,
        &controls.size.mod,
        controls.size.base,
        0.0f, 2.5f
    );

    mod.registerTarget(
        ModTargetID::REVERB_PREDELAY,
        &controls.preDelay.mod,
        controls.preDelay.base,
        0.0f, 2.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_LOW_CUT,
        &controls.lowCut.mod,
        controls.lowCut.base,
        0.0f, 1.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_HIGH_CUT,
        &controls.highCut.mod,
        controls.highCut.base,
        0.0f, 1.0f
    );

    // ---- PLATE ----
    mod.registerTarget(
        ModTargetID::REVERB_PLATE_DAMP,
        &controls.plateDamp.mod,
        controls.plateDamp.base,
        0.0f, 0.99f
    );

    mod.registerTarget(
        ModTargetID::REVERB_PLATE_WIDTH,
        &controls.plateWidth.mod,
        controls.plateWidth.base,
        0.0f, 1.0f
    );

    // ---- HALL ----
    mod.registerTarget(
        ModTargetID::REVERB_HALL_DECAY,
        &controls.hallDecay.mod,
        controls.hallDecay.base,
        0.1f, 10.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_HALL_SPIN_RATE,
        &controls.hallSpinRate.mod,
        controls.hallSpinRate.base,
        0.0f, 5.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_HALL_SPIN_DEPTH,
        &controls.hallSpinDepth.mod,
        controls.hallSpinDepth.base,
        0.0f, 1.0f
    );

    // ---- VINTAGE ----
    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_ER_SIZE,
        &controls.vintageERSize.mod,
        controls.vintageERSize.base,
        0.001f, 0.1f
    );

    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_DAMP,
        &controls.vintageDamp.mod,
        controls.vintageDamp.base,
        0.0f, 0.99f
    );

    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_DIFF_A,
        &controls.vintageDiffA.mod,
        controls.vintageDiffA.base,
        0.0f, 1.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_DIFF_B,
        &controls.vintageDiffB.mod,
        controls.vintageDiffB.base,
        0.0f, 1.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_CHORUS_RATE,
        &controls.vintageChorus.rateMod,
        controls.vintageChorus.rateBase,
        0.0f, 5.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_VINTAGE_CHORUS_DEPTH,
        &controls.vintageChorus.depthMod,
        controls.vintageChorus.depthBase,
        0.0f, 0.05f
    );

    // ---- NITROUS ----
    mod.registerTarget(
        ModTargetID::REVERB_NITROUS_DIFFUSION,
        &controls.nitrousDiff.mod,
        controls.nitrousDiff.base,
        0.0f, 1.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_NITROUS_FEEDBACK,
        &controls.nitrousFeedback.mod,
        controls.nitrousFeedback.base,
        -0.95f, 0.98f
    );

    mod.registerTarget(
        ModTargetID::REVERB_NITROUS_CHORUS_RATE,
        &controls.nitrousChorus.rateMod,
        controls.nitrousChorus.rateBase,
        0.0f, 6.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_NITROUS_CHORUS_DEPTH,
        &controls.nitrousChorus.depthMod,
        controls.nitrousChorus.depthBase,
        0.0f, 0.05f
    );

    // ---- BASIN ----
    mod.registerTarget(
        ModTargetID::REVERB_BASIN_FEEDBACK,
        &controls.basinFeedback.mod,
        controls.basinFeedback.base,
        0.0f, 0.98f
    );

    mod.registerTarget(
        ModTargetID::REVERB_BASIN_CHORUS_RATE,
        &controls.basinChorus.rateMod,
        controls.basinChorus.rateBase,
        0.0f, 4.0f
    );

    mod.registerTarget(
        ModTargetID::REVERB_BASIN_CHORUS_DEPTH,
        &controls.basinChorus.depthMod,
        controls.basinChorus.depthBase,
        0.0f, 0.05f
    );
}
