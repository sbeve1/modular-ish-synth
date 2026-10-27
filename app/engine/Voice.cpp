#include "engine/Voice.h"

#include <algorithm>
#include <cmath>

using dsp::fastTanh;

static float syncHz(float tempo, int div)
{
    div = std::clamp(div, 0, 14);
    return (tempo / 60.0f) / kSyncDivBeats[div];
}

static inline float frac01(float x) { return x - std::floor(x); }

// Bend warp: p^e = exp2(e * log2 p) without a pow per sample per unison voice.
// log2 p comes from a 4096-point table (interpolation error < 1e-3 in log2
// above p = 1/256, falling quadratically); the first 1/256 of the cycle, where
// log2 curves sharply, uses the exact fast log2.
namespace {
constexpr int LOG2_N = 4096;
struct Log2Table {
    float v[LOG2_N + 1];
    Log2Table()
    {
        v[0] = 0.0f;   // unused: small phases take the exact path
        for (int i = 1; i <= LOG2_N; ++i) v[i] = std::log2((float)i / LOG2_N);
    }
} const kLog2;
} // namespace

static inline float bendPhase(float p, float e)
{
    float l;
    if (p < 1.0f / 256.0f) {
        l = dsp::fastLog2(std::max(p, 1e-30f));
    } else {
        float pos = p * LOG2_N;
        int i = (int)pos;
        l = kLog2.v[i] + (kLog2.v[i + 1] - kLog2.v[i]) * (pos - (float)i);
    }
    return dsp::fastExp2Neg(e * l);
}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------
void Voice::start(int n, float vel, float startPitch, uint64_t order, const VoiceContext& ctx, bool retrigger)
{
    const bool wasActive = active;

    note = n;
    velocity = vel;
    targetPitch = (float)n;
    pitch = startPitch;
    startOrder = order;
    gate = true;
    sustained = false;
    active = true;

    if (!retrigger && wasActive)
        return;   // legato: keep envelopes and phases running

    timeOn = 0.0f;
    rng = dsp::Rng((uint32_t)(order * 2654435761u + 12345u));
    randomValue = rng.uni();

    for (auto& e : envs)
        e.noteOn(n, vel);   // retriggers from the current level, so steals don't click

    for (int l = 0; l < 4; ++l) {
        lfo[l].reset(0.0f, rng.next());
        lfo[l].smoothed = 0.0f;
    }

    for (int o = 0; o < 2; ++o) {
        float ph   = ctx.value[(int)oscParam(o, P::OSCA_PHASE)];
        float rand = ctx.value[(int)oscParam(o, P::OSCA_RAND)];
        for (float& p : oscState[o].phase)
            p = frac01(ph + rand * rng.uni());
    }
    subPhase = 0.0f;

    if (!wasActive) {
        ampCur = 0.0f;
        for (auto& f : svf) { f[0].reset(); f[1].reset(); }
        for (float& p : pink) p = 0.0f;
        brown = 0.0f;
    }

    // fresh source values so the first block's modulation is sane
    for (int i = (int)ModSrc::Env1; i <= (int)ModSrc::Lfo4; ++i) src[i] = 0.0f;
    src[(int)ModSrc::Velocity] = vel;
    src[(int)ModSrc::Note]     = std::clamp((n - 60) / 60.0f, -1.0f, 1.0f);
    src[(int)ModSrc::Random]   = randomValue;
}

void Voice::release()
{
    gate = false;
    sustained = false;
    for (auto& e : envs) e.noteOff();
}

float Voice::lfoPhase(int i, const VoiceContext& ctx) const
{
    float offset = eff(ctx, lfoParam(i, P::LFO1_PHASE));
    auto mode = (LfoMode)(int)eff(ctx, lfoParam(i, P::LFO1_MODE));
    float ph = mode == LfoMode::Free ? ctx.freeLfo[i].phase : lfo[i].phase;
    return frac01(std::min(ph, 0.99999f) + offset);
}

// ---------------------------------------------------------------------
// Modulation
// ---------------------------------------------------------------------
float Voice::eff(const VoiceContext& ctx, P p) const
{
    int i = (int)p;
    return ctx.modulated[i] ? modValue[i] : ctx.value[i];
}

void Voice::updateModulation(const VoiceContext& ctx)
{
    for (int s = 0; s < ctx.numSlots; ++s)
        modOff[ctx.slots[s].target] = 0.0f;

    for (int s = 0; s < ctx.numSlots; ++s) {
        const ModSlotData& slot = ctx.slots[s];
        float v = shapeModValue(src[(int)slot.src], slot.curve);
        if (slot.aux != ModSrc::None)
            v *= src[(int)slot.aux];
        float amt = v * slot.amount;
        if (slot.mode == ModMode::Multiply)
            amt *= ctx.norm[slot.target];   // scales the knob position toward zero
        modOff[slot.target] += amt;
    }

    // convert each modulated target to its plain value once per block
    for (int s = 0; s < ctx.numSlots; ++s) {
        int t = ctx.slots[s].target;
        modValue[t] = paramInfo(t).toValue(ctx.norm[t] + modOff[t]);
        float n = std::clamp(ctx.norm[t] + modOff[t], 0.0f, 1.0f);
        modMin[t] = std::min(modMin[t], n);
        modMax[t] = std::max(modMax[t], n);
    }
}

void Voice::updateSources(const VoiceContext& ctx, int n)
{
    const float sr = ctx.sampleRate;
    const float dt = n / sr;
    timeOn += dt;

    for (int e = 0; e < 3; ++e) {
        Envelope& env = envs[e];
        env.attack       = eff(ctx, envParam(e, P::ENV1_ATK));
        env.hold         = eff(ctx, envParam(e, P::ENV1_HOLD));
        env.decay        = eff(ctx, envParam(e, P::ENV1_DEC));
        env.sustain      = eff(ctx, envParam(e, P::ENV1_SUS));
        env.release      = eff(ctx, envParam(e, P::ENV1_REL));
        env.attackCurve  = eff(ctx, envParam(e, P::ENV1_ACRV));
        env.decayCurve   = eff(ctx, envParam(e, P::ENV1_DCRV));
        env.releaseCurve = eff(ctx, envParam(e, P::ENV1_RCRV));
        env.process(sr / (float)n);   // advance one control block
        src[(int)ModSrc::Env1 + e] = env.level();
    }

    for (int l = 0; l < 4; ++l) {
        auto shape  = (LfoShape)(int)eff(ctx, lfoParam(l, P::LFO1_SHAPE));
        auto mode   = (LfoMode)(int)eff(ctx, lfoParam(l, P::LFO1_MODE));
        bool sync   = eff(ctx, lfoParam(l, P::LFO1_SYNC)) > 0.5f;
        float rate  = sync ? syncHz(ctx.tempo, (int)eff(ctx, lfoParam(l, P::LFO1_DIV)))
                           : eff(ctx, lfoParam(l, P::LFO1_RATE));
        float off   = eff(ctx, lfoParam(l, P::LFO1_PHASE));
        float rise  = eff(ctx, lfoParam(l, P::LFO1_RISE));
        float smooth= eff(ctx, lfoParam(l, P::LFO1_SMOOTH));

        float v;
        if (mode == LfoMode::Free) {
            const LfoState& g = ctx.freeLfo[l];
            v = lfoShapeValue(shape, frac01(g.phase + off), g, ctx.lfoCurve[l]);
        } else {
            lfo[l].advance(rate * dt, mode);
            v = lfoShapeValue(shape, frac01(std::min(lfo[l].phase, 0.99999f) + off), lfo[l], ctx.lfoCurve[l]);
        }

        if (smooth > 0.0f) {
            float c = std::exp(-dt / (smooth * 0.25f));
            lfo[l].smoothed = v + c * (lfo[l].smoothed - v);
            v = lfo[l].smoothed;
        } else {
            lfo[l].smoothed = v;
        }

        if (rise > 0.0f)
            v *= std::min(timeOn / rise, 1.0f);

        src[(int)ModSrc::Lfo1 + l] = v;
    }
}

// ---------------------------------------------------------------------
// Oscillator
// ---------------------------------------------------------------------
enum class Warp { Off, Bend, Pwm, Sync, Mirror, Fold };

// Inner oscillator loop, specialized per warp mode so the per-sample work has
// no mode branches. Unison voices are the outer loop: each voice's phase,
// increment and gains stay in registers while it fills the block.
template <Warp W>
static void oscLoop(const float* t0, const float* t1, float ff, float* phase, const float* inc,
                    const float* gl, const float* gr, int uni, int n, float* outL, float* outR,
                    float amt, float bendExp, float pwmW, float syncRatio, float foldGain)
{
    for (int i = 0; i < uni; ++i) {
        float p = phase[i];
        const float dp = inc[i], gL = gl[i], gR = gr[i];
        for (int s = 0; s < n; ++s) {
            float wp;
            if constexpr (W == Warp::Bend)        wp = std::min(bendPhase(p, bendExp), 0.99999f);
            else if constexpr (W == Warp::Pwm)    wp = std::min(p < pwmW ? p * (0.5f / pwmW) : 0.5f + (p - pwmW) * (0.5f / (1.0f - pwmW)), 0.99999f);
            else if constexpr (W == Warp::Sync)   { wp = p * syncRatio; wp -= (float)(int)wp; }
            else if constexpr (W == Warp::Mirror) wp = p + ((p < 0.5f ? p : 1.0f - p) - p) * amt;
            else                                  wp = p;

            float idx = wp * WT_SIZE;
            int   i0 = (int)idx;
            float fr = idx - (float)i0;
            float a = t0[i0] + fr * (t0[i0 + 1] - t0[i0]);
            float b = t1[i0] + fr * (t1[i0 + 1] - t1[i0]);
            float v = a + ff * (b - a);
            if constexpr (W == Warp::Fold) v = dsp::fastSin(v * foldGain);
            outL[s] += v * gL;
            outR[s] += v * gR;

            p += dp;
            if (p >= 1.0f) p -= (float)(int)p;   // safe even if the increment reaches 1 (extreme pitch)
        }
        phase[i] = p;
    }
}

void Voice::renderOsc(const VoiceContext& ctx, int osc, float basePitch,
                      float* dl, float* dr, float* fl, float* fr, int n)
{
    const WavetableSet* wt = ctx.wavetable[osc];
    if (!wt || wt->mips.empty())
        return;

    auto op = [&](P a) { return eff(ctx, oscParam(osc, a)); };
    const float sr = ctx.sampleRate;
    OscState& st = oscState[osc];

    float notePitch = basePitch + op(P::OSCA_OCT) * 12.0f + op(P::OSCA_SEMI) + op(P::OSCA_FINE) * 0.01f;
    float freq  = dsp::midiToHz(notePitch);
    int   uni   = std::clamp((int)op(P::OSCA_UNISON), 1, UNISON_MAX);
    float detune = op(P::OSCA_DETUNE);
    float blend = op(P::OSCA_BLEND);
    float width = op(P::OSCA_WIDTH);
    float pan   = op(P::OSCA_PAN);
    float level = op(P::OSCA_LEVEL);
    auto  warp  = (Warp)(int)op(P::OSCA_WARP);
    float amt   = op(P::OSCA_WARPAMT);
    float pos   = op(P::OSCA_POS);
    st.pos = pos;

    float framePos = pos * (WT_FRAMES - 1);
    int   f0 = std::min((int)framePos, WT_FRAMES - 1);
    int   f1 = std::min(f0 + 1, WT_FRAMES - 1);
    float ff = framePos - f0;

    float syncRatio = 1.0f + amt * 7.0f;
    float bendExp   = std::exp2((amt - 0.5f) * 4.0f);   // 0.25 .. 4, neutral at 50%
    if (warp == Warp::Bend && std::fabs(amt - 0.5f) < 1e-3f) warp = Warp::Off;
    float pwmW      = 0.5f - amt * 0.47f;

    // detune ratios and stereo gains change rarely: recompute only when their inputs do
    if (uni != st.cUni || detune != st.cDetune) {
        float cents = 80.0f * std::pow(detune, 1.5f);
        for (int i = 0; i < uni; ++i) {
            float s = uni == 1 ? 0.0f : (float)i / (uni - 1) * 2.0f - 1.0f;
            st.ratio[i] = std::exp2(s * cents / 1200.0f);
        }
        st.maxRatio = uni > 1 ? st.ratio[uni - 1] : 1.0f;
        st.cDetune = detune;
        st.cBlend = -1.0f;   // force gains below
    }
    if (uni != st.cUni || blend != st.cBlend || width != st.cWidth || pan != st.cPan || level != st.cLevel) {
        float sumSq = 0.0f;
        for (int i = 0; i < uni; ++i) {
            float s = uni == 1 ? 0.0f : (float)i / (uni - 1) * 2.0f - 1.0f;
            bool center = uni == 1 || std::fabs(s) <= 1.0f / (uni - 1) + 1e-4f;
            float g = center ? 1.0f : blend;
            sumSq += g * g;
            float l, r;
            dsp::panGains(std::clamp(pan + s * width, -1.0f, 1.0f), l, r);
            st.gl[i] = g * l;
            st.gr[i] = g * r;
        }
        float norm = level / std::sqrt(std::max(sumSq, 1e-6f)) * 1.41421f;   // pan law compensation
        for (int i = 0; i < uni; ++i) { st.gl[i] *= norm; st.gr[i] *= norm; }
        st.cBlend = blend; st.cWidth = width; st.cPan = pan; st.cLevel = level;
    }
    st.cUni = uni;

    float inc[UNISON_MAX];
    const float base = freq / sr;
    for (int i = 0; i < uni; ++i) inc[i] = base * st.ratio[i];

    float lvlFreq  = freq * st.maxRatio * (warp == Warp::Sync ? syncRatio : 1.0f);
    int   level_   = wtLevelFor(lvlFreq, sr);
    const float* t0 = wt->table(level_, f0);
    const float* t1 = wt->table(level_, f1);

    const bool toFilter = op(P::OSCA_FILTER) > 0.5f;
    float* outL = toFilter ? fl : dl;
    float* outR = toFilter ? fr : dr;
    const float foldGain = (1.0f + amt * 5.0f) * dsp::PI * 0.5f;

#define NW_OSC(W) oscLoop<W>(t0, t1, ff, st.phase, inc, st.gl, st.gr, uni, n, outL, outR, amt, bendExp, pwmW, syncRatio, foldGain)
    switch (warp) {
        case Warp::Bend:   NW_OSC(Warp::Bend); break;
        case Warp::Pwm:    NW_OSC(Warp::Pwm); break;
        case Warp::Sync:   NW_OSC(Warp::Sync); break;
        case Warp::Mirror: NW_OSC(Warp::Mirror); break;
        case Warp::Fold:   NW_OSC(Warp::Fold); break;
        default:           NW_OSC(Warp::Off); break;
    }
#undef NW_OSC
}

// ---------------------------------------------------------------------
// Filter (in place). Stage 2 adds the resonant half of a 24 dB cascade.
// ---------------------------------------------------------------------
template <int MODE>   // 0 lp, 1 bp (unity gain), 2 hp, 3 notch, 4 peak
static inline float pickOut(float lp, float bp, float hp, float k)
{
    if constexpr (MODE == 0) return lp;
    else if constexpr (MODE == 1) return bp * k;
    else if constexpr (MODE == 2) return hp;
    else if constexpr (MODE == 3) return lp + hp;
    else return lp - hp;
}

template <int MODE, bool STEEP, bool DRIVE>
static void filterLoop(dsp::Svf* st, float* L, float* R, int n, float g, float gNorm, float mix)
{
    for (int s = 0; s < n; ++s) {
        float in[2] = { L[s], R[s] };
        for (int ch = 0; ch < 2; ++ch) {
            float x = in[ch];
            if constexpr (DRIVE) x = fastTanh(x * g) * gNorm * 1.5f;
            float lp, bp, hp;
            st[0].tick(ch, x, lp, bp, hp);
            float y = pickOut<MODE>(lp, bp, hp, st[0].k);
            if constexpr (STEEP) {
                st[1].tick(ch, y, lp, bp, hp);
                y = pickOut<MODE>(lp, bp, hp, st[1].k);
            }
            (ch == 0 ? L[s] : R[s]) = in[ch] + (y - in[ch]) * mix;
        }
    }
}
void Voice::runFilter(const VoiceContext& ctx, int f, float* L, float* R, int n)
{
    const float sr = ctx.sampleRate;
    auto fp = [&](P p) { return eff(ctx, fltParam(f, p)); };

    int type = (int)fp(P::FLT_TYPE);
    dsp::Svf* st = svf[f];
    if (type != lastFilterType[f]) { st[0].reset(); st[1].reset(); lastFilterType[f] = type; }

    float keytrk = fp(P::FLT_KEYTRK);
    float cutoff = fp(P::FLT_CUTOFF) * std::exp2((pitch - 60.0f) / 12.0f * keytrk);
    float res    = fp(P::FLT_RES);
    float drive  = fp(P::FLT_DRIVE);
    float mix    = fp(P::FLT_MIX);

    bool steep = type == 1 || type == 3 || type == 5;
    int  mode  = type < 2 ? 0 : type < 4 ? 1 : type < 6 ? 2 : type == 6 ? 3 : 4;
    if (steep) {
        st[0].set(cutoff, 1.8478f, sr);
        st[1].set(cutoff, dsp::lerp(0.7654f, 0.05f, res), sr);
    } else {
        st[0].set(cutoff, dsp::lerp(2.0f, 0.05f, res), sr);
    }

    float g = 1.0f + drive * 12.0f;
    float gNorm = 1.0f / std::sqrt(g);

#define NW_FLT(M, S, D) filterLoop<M, S, D>(st, L, R, n, g, gNorm, mix)
    const bool drv = drive > 0.0f;
    switch (mode * 4 + (steep ? 2 : 0) + (drv ? 1 : 0)) {
        case 0:  NW_FLT(0, false, false); break;  case 1:  NW_FLT(0, false, true); break;
        case 2:  NW_FLT(0, true, false);  break;  case 3:  NW_FLT(0, true, true);  break;
        case 4:  NW_FLT(1, false, false); break;  case 5:  NW_FLT(1, false, true); break;
        case 6:  NW_FLT(1, true, false);  break;  case 7:  NW_FLT(1, true, true);  break;
        case 8:  NW_FLT(2, false, false); break;  case 9:  NW_FLT(2, false, true); break;
        case 10: NW_FLT(2, true, false);  break;  case 11: NW_FLT(2, true, true);  break;
        case 12: NW_FLT(3, false, false); break;  case 13: NW_FLT(3, false, true); break;
        case 14: NW_FLT(3, true, false);  break;  case 15: NW_FLT(3, true, true);  break;
        case 16: NW_FLT(4, false, false); break;  case 17: NW_FLT(4, false, true); break;
        case 18: NW_FLT(4, true, false);  break;  default: NW_FLT(4, true, true);  break;
    }
#undef NW_FLT
}

// ---------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------
void Voice::render(const VoiceContext& ctx, float* outL, float* outR, int n)
{
    const float sr = ctx.sampleRate;

    for (int i = (int)ModSrc::ModWheel; i < NUM_MOD_SRC; ++i)
        if (i != (int)ModSrc::Random)
            src[i] = ctx.globalSrc[i];

    updateModulation(ctx);
    updateSources(ctx, n);

    // ---- pitch ----
    float glide = eff(ctx, P::GLIDE);
    if (glide < 1e-4f) pitch = targetPitch;
    else               pitch += (targetPitch - pitch) * (1.0f - std::exp(-(float)n / (glide * sr)));
    float basePitch = pitch + src[(int)ModSrc::PitchBend] * eff(ctx, P::PB_RANGE);

    float dl[CONTROL_RATE] = {}, dr[CONTROL_RATE] = {};   // straight to amp
    float fl[CONTROL_RATE] = {}, fr[CONTROL_RATE] = {};   // through the filter

    // ---- oscillators ----
    for (int o = 0; o < 2; ++o)
        if (eff(ctx, oscParam(o, P::OSCA_ON)) > 0.5f)
            renderOsc(ctx, o, basePitch, dl, dr, fl, fr, n);

    // ---- sub ----
    if (eff(ctx, P::SUB_ON) > 0.5f) {
        float f   = dsp::midiToHz(basePitch + eff(ctx, P::SUB_OCT) * 12.0f);
        float inc = std::min(f / sr, 0.45f);
        int shape = (int)eff(ctx, P::SUB_SHAPE);
        float gl, gr;
        dsp::panGains(eff(ctx, P::SUB_PAN), gl, gr);
        float lv = eff(ctx, P::SUB_LEVEL) * 1.41421f;
        float* L = eff(ctx, P::SUB_FILTER) > 0.5f ? fl : dl;
        float* R = eff(ctx, P::SUB_FILTER) > 0.5f ? fr : dr;
        for (int s = 0; s < n; ++s) {
            float p = subPhase, v;
            switch (shape) {
                case 0:  v = dsp::fastSin(dsp::TWO_PI * p); break;
                case 1:  v = p < 0.25f ? 4 * p : p < 0.75f ? 2 - 4 * p : 4 * p - 4; break;
                case 2:  v = 1.0f - 2.0f * p + dsp::polyBlep(p, inc); break;
                default: v = (p < 0.5f ? 1.0f : -1.0f) + dsp::polyBlep(p, inc) - dsp::polyBlep(frac01(p + 0.5f), inc); break;
            }
            v *= lv;
            L[s] += v * gl;
            R[s] += v * gr;
            subPhase = p + inc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
        }
    }

    // ---- noise ----
    if (eff(ctx, P::NOISE_ON) > 0.5f) {
        int type = (int)eff(ctx, P::NOISE_TYPE);
        float gl, gr;
        dsp::panGains(eff(ctx, P::NOISE_PAN), gl, gr);
        float lv = eff(ctx, P::NOISE_LEVEL) * 1.41421f;
        float* L = eff(ctx, P::NOISE_FILTER) > 0.5f ? fl : dl;
        float* R = eff(ctx, P::NOISE_FILTER) > 0.5f ? fr : dr;
        for (int s = 0; s < n; ++s) {
            float w = rng.bi(), v;
            if (type == 1) {   // Paul Kellet pink
                pink[0] = 0.99886f * pink[0] + w * 0.0555179f;
                pink[1] = 0.99332f * pink[1] + w * 0.0750759f;
                pink[2] = 0.96900f * pink[2] + w * 0.1538520f;
                pink[3] = 0.86650f * pink[3] + w * 0.3104856f;
                pink[4] = 0.55000f * pink[4] + w * 0.5329522f;
                pink[5] = -0.7616f * pink[5] - w * 0.0168980f;
                v = (pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6] + w * 0.5362f) * 0.11f;
                pink[6] = w * 0.115926f;
            } else if (type == 2) {
                brown = (brown + 0.02f * w) / 1.02f;
                v = brown * 3.5f;
            } else {
                v = w;
            }
            v *= lv;
            L[s] += v * gl;
            R[s] += v * gr;
        }
    }

    // ---- filters: 1 and 2, serial or parallel ----
    {
        bool on1 = eff(ctx, P::FLT_ON) > 0.5f, on2 = eff(ctx, P::FLT2_ON) > 0.5f;
        bool parallel = eff(ctx, P::FLT_ROUTING) > 0.5f;
        if (on1 && on2 && parallel) {
            float l2[CONTROL_RATE], r2[CONTROL_RATE];
            std::copy(fl, fl + n, l2);
            std::copy(fr, fr + n, r2);
            runFilter(ctx, 0, fl, fr, n);
            runFilter(ctx, 1, l2, r2, n);
            for (int s = 0; s < n; ++s) { fl[s] = (fl[s] + l2[s]) * 0.7071f; fr[s] = (fr[s] + r2[s]) * 0.7071f; }
        } else {
            if (on1) runFilter(ctx, 0, fl, fr, n);
            if (on2) runFilter(ctx, 1, fl, fr, n);
        }
    }

    // ---- amp ----
    float velSens = eff(ctx, P::VEL_SENS);
    float velGain = 1.0f - velSens * (1.0f - velocity);
    float ampTarget = src[(int)ModSrc::Env1] * velGain;
    float step = (ampTarget - ampCur) / (float)n;
    float a = ampCur;
    for (int s = 0; s < n; ++s) {
        a += step;
        outL[s] += (dl[s] + fl[s]) * a;
        outR[s] += (dr[s] + fr[s]) * a;
    }
    ampCur = ampTarget;

    if (!envs[0].isActive())
        active = false;
}
