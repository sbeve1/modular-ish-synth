#include "engine/Instrument.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------
// Mod source names
// ---------------------------------------------------------------------
static const char* const kSrcNames[NUM_MOD_SRC] = {
    "None", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
    "Velocity", "Note", "Mod Wheel", "Aftertouch", "Pitch Bend", "Random",
    "Macro 1", "Macro 2", "Macro 3", "Macro 4",
};
static const char* const kSrcShort[NUM_MOD_SRC] = {
    "-", "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4",
    "VEL", "NOTE", "MW", "AT", "PB", "RAND", "M1", "M2", "M3", "M4",
};

const char* modSrcName(ModSrc s)      { return kSrcNames[(int)s]; }
const char* modSrcShortName(ModSrc s) { return kSrcShort[(int)s]; }
bool modSrcBipolar(ModSrc s)
{
    return (s >= ModSrc::Lfo1 && s <= ModSrc::Lfo4) || s == ModSrc::Note || s == ModSrc::PitchBend;
}

static const char* const kModeNames[]  = { "Add", "Multiply" };
static const char* const kCurveNames[] = { "Linear", "Exp", "Soft", "Hard" };
const char* modModeName(ModMode m)   { return kModeNames[(int)m]; }
const char* modCurveName(ModCurve c) { return kCurveNames[(int)c]; }

// ---------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------
void Instrument::prepare(float sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = maxBlockSize;
    busL.assign(maxBlock, 0.0f);
    busR.assign(maxBlock, 0.0f);

    dist.prepare(sr);
    chorus.prepare(sr);
    delay.prepare(sr);
    reverb.prepare(sr);
    limiter.prepare(sr);
    masterGain.setTime(0.03f, sr);
    masterGain.snap(dsp::dbToGain(s.params.value(P::MASTER_VOL)));

    for (int l = 0; l < 4; ++l) freeLfo[l].reset(0.0f, 1000u + l);
    for (auto& v : voices) v.active = false;
}

// ---------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------
void Instrument::handleEvent(const Event& e)
{
    switch (e.type) {
        case EventType::NoteOn:
            if (e.b == 0) noteOff(e.a);
            else          noteOn(e.a, e.b / 127.0f);
            break;
        case EventType::NoteOff:
            noteOff(e.a);
            break;
        case EventType::PitchBend:
            pitchBend = std::clamp(e.f, -1.0f, 1.0f);
            break;
        case EventType::Aftertouch:
            aftertouch = std::clamp(e.f, 0.0f, 1.0f);
            break;
        case EventType::ControlChange:
            if (e.a == 1) {
                modWheel = e.b / 127.0f;
            } else if (e.a == 64) {
                bool down = e.b >= 64;
                if (sustainPedal && !down) {
                    for (auto& v : voices)
                        if (v.active && v.sustained) releaseVoice(v);
                }
                sustainPedal = down;
            } else if (e.a == 120 || e.a == 123) {
                allNotesOff();
            }
            break;
        case EventType::AllNotesOff:
            allNotesOff();
            break;
    }
}

void Instrument::allNotesOff()
{
    for (auto& v : voices)
        if (v.active && v.gate) releaseVoice(v);
    std::memset(keyDown, 0, sizeof keyDown);
    heldCount = 0;
    sustainPedal = false;
}

void Instrument::releaseVoice(Voice& v)
{
    v.release();
}

int Instrument::pickVoice()
{
    int poly = std::clamp((int)value[(int)P::POLYPHONY], 1, MAX_VOICES);

    for (int i = 0; i < poly; ++i)
        if (!voices[i].active) return i;

    // steal: quietest released voice first, else the oldest
    int best = -1;
    float quietest = 1e9f;
    for (int i = 0; i < poly; ++i)
        if (!voices[i].gate && voices[i].amplitude() < quietest) { quietest = voices[i].amplitude(); best = i; }
    if (best >= 0) return best;

    uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < poly; ++i)
        if (voices[i].startOrder < oldest) { oldest = voices[i].startOrder; best = i; }
    return best;
}

void Instrument::noteOn(int note, float vel)
{
    note = std::clamp(note, 0, 127);
    if (!keyDown[note] && heldCount < 128) heldStack[heldCount++] = (uint8_t)note;
    keyDown[note] = true;

    const int mode = (int)value[(int)P::VOICE_MODE];   // 0 poly, 1 mono, 2 legato
    const bool glide = value[(int)P::GLIDE] > 1e-4f;

    if (mode == 0) {
        int i = pickVoice();
        float start = (glide && lastPitch >= 0.0f) ? lastPitch : (float)note;
        voices[i].start(note, vel, start, ++noteCounter, ctx, true);
        displayVoice = i;
    } else {
        Voice& v = voices[0];
        bool legato = mode == 2 && v.active && v.gate;
        float start = v.active && glide ? v.currentPitch() : (float)note;
        if (legato) v.glideTo(note);
        else        v.start(note, vel, start, ++noteCounter, ctx, true);
        if (!glide) v.glideTo(note);   // jump immediately when glide is off
        displayVoice = 0;
    }
    lastPitch = (float)note;
}

void Instrument::noteOff(int note)
{
    note = std::clamp(note, 0, 127);
    keyDown[note] = false;
    for (int i = 0; i < heldCount; ++i)
        if (heldStack[i] == note) {
            std::memmove(&heldStack[i], &heldStack[i + 1], heldCount - i - 1);
            --heldCount;
            break;
        }

    const int mode = (int)value[(int)P::VOICE_MODE];
    if (mode != 0) {
        Voice& v = voices[0];
        if (!v.active || v.note != note) return;
        if (heldCount > 0) {                    // fall back to the last held key
            v.glideTo(heldStack[heldCount - 1]);
            lastPitch = (float)heldStack[heldCount - 1];
        } else if (sustainPedal) {
            v.sustained = true;
        } else {
            releaseVoice(v);
        }
        return;
    }

    for (auto& v : voices) {
        if (v.active && v.gate && !v.sustained && v.note == note) {
            if (sustainPedal) v.sustained = true;
            else              releaseVoice(v);
        }
    }
}

// ---------------------------------------------------------------------
// Per-block snapshot of params and mod slots
// ---------------------------------------------------------------------
void Instrument::snapshot()
{
    for (int i = 0; i < NUM_PARAMS; ++i) {
        norm[i] = s.params.norm((P)i);
        value[i] = paramInfo(i).toValue(norm[i]);
        modulated[i] = false;
    }

    numVoiceSlots = numGlobalSlots = 0;
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) {
        ModSlotData d = s.mods.get(i);
        if (!d.active() || d.target >= NUM_PARAMS || d.amount == 0.0f) continue;
        modulated[d.target] = true;
        if (paramInfo(d.target).scope == Scope::Voice) slots[numVoiceSlots++] = d;
        else                                            globalSlots[numGlobalSlots++] = d;
    }

    ctx.sampleRate = sr;
    ctx.tempo = value[(int)P::TEMPO];
    ctx.norm = norm;
    ctx.value = value;
    ctx.modulated = modulated;
    ctx.slots = slots;
    ctx.numSlots = numVoiceSlots;
    ctx.freeLfo = freeLfo;
    for (int o = 0; o < 2; ++o)
        ctx.wavetable[o] = s.wavetables[o].load(std::memory_order_acquire);

    ctx.globalSrc[(int)ModSrc::ModWheel]   = modWheel;
    ctx.globalSrc[(int)ModSrc::Aftertouch] = aftertouch;
    ctx.globalSrc[(int)ModSrc::PitchBend]  = pitchBend;
    for (int m = 0; m < 4; ++m)
        ctx.globalSrc[(int)ModSrc::Macro1 + m] = value[(int)P::MACRO1 + m];
}

// Global params (FX, master) are modulated with the newest voice's sources.
void Instrument::updateGlobalModulation()
{
    std::memcpy(gvalue, value, sizeof gvalue);
    if (numGlobalSlots == 0) return;

    float src[NUM_MOD_SRC];
    std::memcpy(src, ctx.globalSrc, sizeof src);
    if (displayVoice >= 0 && voices[displayVoice].active)
        for (int i = (int)ModSrc::Env1; i <= (int)ModSrc::Random; ++i)
            if (i < (int)ModSrc::ModWheel || i > (int)ModSrc::PitchBend)
                src[i] = voices[displayVoice].src[i];

    float off[NUM_PARAMS];
    for (int k = 0; k < numGlobalSlots; ++k) off[globalSlots[k].target] = 0.0f;
    for (int k = 0; k < numGlobalSlots; ++k) {
        const ModSlotData& d = globalSlots[k];
        float v = shapeModValue(src[(int)d.src], d.curve);
        if (d.aux != ModSrc::None) v *= src[(int)d.aux];
        float amt = v * d.amount;
        if (d.mode == ModMode::Multiply) amt *= norm[d.target];
        off[d.target] += amt;
    }
    for (int k = 0; k < numGlobalSlots; ++k) {
        int t = globalSlots[k].target;
        gvalue[t] = paramInfo(t).toValue(norm[t] + off[t]);
    }
}

void Instrument::advanceFreeLfos(int n)
{
    for (int l = 0; l < 4; ++l) {
        bool sync = value[(int)lfoParam(l, P::LFO1_SYNC)] > 0.5f;
        float rate = sync ? (ctx.tempo / 60.0f) / kSyncDivBeats[std::clamp((int)value[(int)lfoParam(l, P::LFO1_DIV)], 0, 14)]
                          : value[(int)lfoParam(l, P::LFO1_RATE)];
        freeLfo[l].advance(rate * n / sr, LfoMode::Free);
    }
}

// ---------------------------------------------------------------------
// Process
// ---------------------------------------------------------------------
void Instrument::process(float* outL, float* outR, int frames)
{
    auto t0 = std::chrono::steady_clock::now();

    snapshot();

    Event e;
    while (s.midiEvents.pop(e)) handleEvent(e);
    while (s.uiEvents.pop(e))   handleEvent(e);

    int done = 0;
    while (done < frames) {
        int chunk = std::min(frames - done, maxBlock);
        float* L = busL.data();
        float* R = busR.data();
        std::fill(L, L + chunk, 0.0f);
        std::fill(R, R + chunk, 0.0f);

        for (int pos = 0; pos < chunk; pos += CONTROL_RATE) {
            int n = std::min(CONTROL_RATE, chunk - pos);
            for (auto& v : voices)
                if (v.active) v.render(ctx, L + pos, R + pos, n);
            advanceFreeLfos(n);
        }

        updateGlobalModulation();
        renderFx(L, R, chunk);

        std::memcpy(outL + done, L, chunk * sizeof(float));
        std::memcpy(outR + done, R, chunk * sizeof(float));
        done += chunk;
    }

    double busy = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    publish(outL, outR, frames, busy);
}

void Instrument::renderFx(float* l, float* r, int n)
{
    // voice bus headroom
    for (int i = 0; i < n; ++i) { l[i] *= 0.5f; r[i] *= 0.5f; }

    const bool on[4] = { gv(P::DIST_ON) > 0.5f, gv(P::CHO_ON) > 0.5f, gv(P::DLY_ON) > 0.5f, gv(P::REV_ON) > 0.5f };
    if (on[0] && !fxWasOn[0]) dist.reset();
    if (on[1] && !fxWasOn[1]) chorus.reset();
    if (on[2] && !fxWasOn[2]) delay.reset();
    if (on[3] && !fxWasOn[3]) reverb.reset();
    std::copy(on, on + 4, fxWasOn);

    if (on[0]) dist.process(l, r, n, { (int)gv(P::DIST_MODE), gv(P::DIST_DRIVE), gv(P::DIST_TONE), gv(P::DIST_MIX) });
    if (on[1]) chorus.process(l, r, n, { gv(P::CHO_RATE), gv(P::CHO_DEPTH), gv(P::CHO_FB), gv(P::CHO_MIX) });
    if (on[2]) {
        float t = gv(P::DLY_SYNC) > 0.5f
            ? (60.0f / ctx.tempo) * kSyncDivBeats[std::clamp((int)gv(P::DLY_DIV), 0, 14)]
            : gv(P::DLY_TIME) * 0.001f;
        delay.process(l, r, n, { t, gv(P::DLY_FB), gv(P::DLY_TONE), gv(P::DLY_MIX), gv(P::DLY_PING) > 0.5f });
    }
    if (on[3]) reverb.process(l, r, n, { gv(P::REV_SIZE), gv(P::REV_DECAY), gv(P::REV_DAMP),
                                         gv(P::REV_PREDLY), gv(P::REV_WIDTH), gv(P::REV_MIX) });

    float target = dsp::dbToGain(gv(P::MASTER_VOL));
    for (int i = 0; i < n; ++i) {
        float g = masterGain.next(target);
        l[i] *= g;
        r[i] *= g;
    }
    limiter.process(l, r, n, 0.966f);   // -0.3 dBFS
}

// ---------------------------------------------------------------------
// Telemetry (for the UI only)
// ---------------------------------------------------------------------
void Instrument::publish(const float* l, const float* r, int n, double busySeconds)
{
    Telemetry& t = s.telemetry;

    uint32_t w = t.scopeWrite.load(std::memory_order_relaxed);
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < n; ++i) {
        t.scope[(w + i) & (Telemetry::SCOPE_SIZE - 1)] = 0.5f * (l[i] + r[i]);
        pl = std::max(pl, std::fabs(l[i]));
        pr = std::max(pr, std::fabs(r[i]));
    }
    t.scopeWrite.store(w + n, std::memory_order_release);

    if (pl > t.peakL.load(std::memory_order_relaxed)) t.peakL.store(pl, std::memory_order_relaxed);
    if (pr > t.peakR.load(std::memory_order_relaxed)) t.peakR.store(pr, std::memory_order_relaxed);

    float load = (float)(busySeconds / (n / (double)sr));
    t.cpuLoad.store(0.9f * t.cpuLoad.load(std::memory_order_relaxed) + 0.1f * load, std::memory_order_relaxed);

    int count = 0;
    for (auto& v : voices) count += v.active ? 1 : 0;
    t.activeVoices.store(count, std::memory_order_relaxed);

    uint32_t mask[4] = {};
    for (int k = 0; k < 128; ++k) if (keyDown[k]) mask[k >> 5] |= 1u << (k & 31);
    for (auto& v : voices) if (v.active && v.gate) mask[v.note >> 5] |= 1u << (v.note & 31);
    for (int k = 0; k < 4; ++k) t.notes[k].store(mask[k], std::memory_order_relaxed);

    // newest voice -> live modulation displays
    if (displayVoice >= 0 && !voices[displayVoice].active) {
        // fall back to any active voice, newest first
        uint64_t newest = 0;
        displayVoice = -1;
        for (int i = 0; i < MAX_VOICES; ++i)
            if (voices[i].active && voices[i].startOrder >= newest) { newest = voices[i].startOrder; displayVoice = i; }
    }

    bool has = displayVoice >= 0;
    t.displayVoiceActive.store(has ? 1 : 0, std::memory_order_relaxed);
    for (int i = 0; i < NUM_MOD_SRC; ++i)
        t.srcValue[i].store(has ? voices[displayVoice].src[i] : ctx.globalSrc[i], std::memory_order_relaxed);

    for (int i = 0; i < NUM_PARAMS; ++i) {
        float nv = norm[i];
        if (modulated[i]) {
            if (paramInfo(i).scope == Scope::Global) nv = paramInfo(i).toNorm(gvalue[i]);
            else if (has) nv = std::clamp(norm[i] + voices[displayVoice].modOff[i], 0.0f, 1.0f);
        }
        t.modNorm[i].store(nv, std::memory_order_relaxed);
    }

    for (int l = 0; l < 4; ++l)
        t.lfoPhase[l].store(has ? voices[displayVoice].lfoPhase(l, ctx) : freeLfo[l].phase, std::memory_order_relaxed);
    for (int e = 0; e < 3; ++e) {
        t.envStage[e].store(has ? voices[displayVoice].env(e).stageIndex() : 0, std::memory_order_relaxed);
        t.envStageTime[e].store(has ? voices[displayVoice].env(e).stageTime() : 0.0f, std::memory_order_relaxed);
    }
    for (int o = 0; o < 2; ++o)
        t.oscPos[o].store(has ? voices[displayVoice].wtPos(o) : value[(int)oscParam(o, P::OSCA_POS)], std::memory_order_relaxed);

    t.callbacks.fetch_add(1, std::memory_order_release);
}
