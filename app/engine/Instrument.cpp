#include "engine/Instrument.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <xmmintrin.h>

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
    phaser.prepare(sr);
    flanger.prepare(sr);
    eq.prepare(sr);
    comp.prepare(sr);
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
    arpReleaseAll();
    arp.numLatched = 0;
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

// Physical key down: track it, then either feed the arpeggiator or play it.
void Instrument::noteOn(int note, float vel)
{
    note = std::clamp(note, 0, 127);
    const bool firstKey = heldCount == 0;
    if (!keyDown[note] && heldCount < 128) heldStack[heldCount++] = (uint8_t)note;
    keyDown[note] = true;
    noteVel[note] = vel;

    if (arpOn()) {
        if (value[(int)P::ARP_HOLD] > 0.5f) {
            if (firstKey) arp.numLatched = 0;   // a new chord replaces the latched one
            bool present = false;
            for (int i = 0; i < arp.numLatched; ++i) present |= arp.latched[i] == note;
            if (!present && arp.numLatched < 128) arp.latched[arp.numLatched++] = (uint8_t)note;
        }
        if (arp.numSounding == 0 && firstKey) { arp.untilNext = 0.0; arp.step = 0; }   // start on the beat
        return;
    }
    voiceOn(note, vel);
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

    if (arpOn()) return;   // the arpeggiator notices the shorter key list on its next step
    voiceOff(note, false);
}

void Instrument::voiceOn(int note, float vel)
{
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

void Instrument::voiceOff(int note, bool arpNote)
{
    const int mode = (int)value[(int)P::VOICE_MODE];
    if (mode != 0) {
        Voice& v = voices[0];
        if (!v.active || v.note != note) return;
        if (!arpNote && heldCount > 0) {          // fall back to the last held key
            v.glideTo(heldStack[heldCount - 1]);
            lastPitch = (float)heldStack[heldCount - 1];
        } else if (!arpNote && sustainPedal) {
            v.sustained = true;
        } else {
            releaseVoice(v);
        }
        return;
    }

    for (auto& v : voices) {
        if (v.active && v.gate && !v.sustained && v.note == note) {
            if (sustainPedal && !arpNote) v.sustained = true;
            else                          releaseVoice(v);
        }
    }
}

// ---------------------------------------------------------------------
// Arpeggiator
// ---------------------------------------------------------------------
void Instrument::arpReleaseAll()
{
    for (int i = 0; i < arp.numSounding; ++i) voiceOff(arp.sounding[i], true);
    arp.numSounding = 0;
}

void Instrument::arpTick(int n)
{
    if (!arpOn()) {
        if (arp.wasOn) { arpReleaseAll(); arp.numLatched = 0; arp.wasOn = false; }
        return;
    }
    if (!arp.wasOn) {
        // switching on while keys are held: take them over from the voices
        for (int i = 0; i < heldCount; ++i) voiceOff(heldStack[i], true);
        arp.wasOn = true;
        arp.untilNext = 0.0;
        arp.step = 0;
    }

    // source notes: latched chord, or the keys held right now (press order)
    const bool hold = value[(int)P::ARP_HOLD] > 0.5f;
    const uint8_t* src = hold ? arp.latched : heldStack;
    int count = hold ? arp.numLatched : heldCount;
    if (count == 0) {
        arpReleaseAll();
        arp.step = 0;
        arp.untilNext = 0.0;
        return;
    }

    arp.gateLeft -= n;
    if (arp.numSounding > 0 && arp.gateLeft <= 0.0) arpReleaseAll();

    arp.untilNext -= n;
    if (arp.untilNext > 0.0) return;

    const int mode = (int)value[(int)P::ARP_MODE];   // Up, Down, Up/Down, Random, As Played, Chord
    const int octaves = std::clamp((int)value[(int)P::ARP_OCT], 1, 4);
    const double stepLen = (60.0 / ctx.tempo) * kSyncDivBeats[std::clamp((int)value[(int)P::ARP_DIV], 0, 14)] * sr;
    const float swing = value[(int)P::ARP_SWING];

    uint8_t base[128];
    std::memcpy(base, src, count);
    if (mode != 4) std::sort(base, base + count);   // As Played keeps press order

    arpReleaseAll();
    if (mode == 5) {   // chord: everything at once, octave rising per step
        int shift = 12 * (arp.step % octaves);
        for (int i = 0; i < count && arp.numSounding < 16; ++i) {
            int note = base[i] + shift;
            if (note > 127) continue;
            voiceOn(note, noteVel[base[i]]);
            arp.sounding[arp.numSounding++] = note;
        }
    } else {
        int total = count * octaves, idx;
        int k = arp.step % total;
        switch (mode) {
            case 1:  idx = total - 1 - k; break;
            case 2: {
                int period = total > 1 ? 2 * total - 2 : 1;
                int q = arp.step % period;
                idx = q < total ? q : period - q;
                break;
            }
            case 3:  idx = (int)(arp.rng.uni() * total) % total; break;
            default: idx = k; break;
        }
        int srcNote = base[idx % count];
        int note = std::min(srcNote + 12 * (idx / count), 127);
        voiceOn(note, noteVel[srcNote]);
        arp.sounding[0] = note;
        arp.numSounding = 1;
    }

    // swing lengthens even steps and shortens odd ones
    double len = stepLen * ((arp.step % 2 == 0) ? (1.0 + swing) : (1.0 - swing));
    arp.gateLeft = len * value[(int)P::ARP_GATE];
    arp.untilNext += len;
    if (arp.untilNext < 0.0) arp.untilNext = 0.0;
    ++arp.step;
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
    for (int l = 0; l < 4; ++l)
        ctx.lfoCurve[l] = s.lfoCurves[l].load(std::memory_order_acquire);

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

    // flush denormals to zero on this thread: decaying filter, reverb and
    // envelope states would otherwise hit very slow subnormal arithmetic
    _mm_setcsr(_mm_getcsr() | 0x8040);

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

        // voices in control blocks, then FX every FX_BLOCK samples so that
        // modulated FX parameters update smoothly regardless of buffer size
        constexpr int FX_BLOCK = 64;
        for (int fxPos = 0; fxPos < chunk; fxPos += FX_BLOCK) {
            int fxEnd = std::min(fxPos + FX_BLOCK, chunk);
            for (int pos = fxPos; pos < fxEnd; pos += CONTROL_RATE) {
                int n = std::min(CONTROL_RATE, fxEnd - pos);
                arpTick(n);
                for (auto& v : voices)
                    if (v.active) v.render(ctx, L + pos, R + pos, n);
                advanceFreeLfos(n);
            }
            updateGlobalModulation();
            renderFx(L + fxPos, R + fxPos, fxEnd - fxPos);
        }

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

    const uint32_t order = s.fxOrder.load(std::memory_order_relaxed);
    for (int slot = 0; slot < NUM_FX; ++slot) {
        FxId id = fxAt(order, slot);
        if ((int)id >= NUM_FX) continue;
        bool on = gv(fxPowerParam(id)) > 0.5f;
        bool was = fxWasOn[(int)id];
        fxWasOn[(int)id] = on;
        if (!on) continue;

        switch (id) {
            case FxId::Distortion:
                if (!was) dist.reset();
                dist.process(l, r, n, { (int)gv(P::DIST_MODE), gv(P::DIST_DRIVE), gv(P::DIST_TONE), gv(P::DIST_MIX) });
                break;
            case FxId::Phaser:
                if (!was) phaser.reset();
                phaser.process(l, r, n, { gv(P::PHA_RATE), gv(P::PHA_DEPTH), gv(P::PHA_FB), gv(P::PHA_MIX) });
                break;
            case FxId::Flanger:
                if (!was) flanger.reset();
                flanger.process(l, r, n, { gv(P::FLG_RATE), gv(P::FLG_DEPTH), gv(P::FLG_FB), gv(P::FLG_MIX) });
                break;
            case FxId::Chorus:
                if (!was) chorus.reset();
                chorus.process(l, r, n, { gv(P::CHO_RATE), gv(P::CHO_DEPTH), gv(P::CHO_FB), gv(P::CHO_MIX) });
                break;
            case FxId::Delay: {
                if (!was) delay.reset();
                float t = gv(P::DLY_SYNC) > 0.5f
                    ? (60.0f / ctx.tempo) * kSyncDivBeats[std::clamp((int)gv(P::DLY_DIV), 0, 14)]
                    : gv(P::DLY_TIME) * 0.001f;
                delay.process(l, r, n, { t, gv(P::DLY_FB), gv(P::DLY_TONE), gv(P::DLY_MIX), gv(P::DLY_PING) > 0.5f });
                break;
            }
            case FxId::Reverb:
                if (!was) reverb.reset();
                reverb.process(l, r, n, { gv(P::REV_SIZE), gv(P::REV_DECAY), gv(P::REV_DAMP),
                                          gv(P::REV_PREDLY), gv(P::REV_WIDTH), gv(P::REV_MIX) });
                break;
            case FxId::Eq:
                if (!was) eq.reset();
                eq.process(l, r, n, { gv(P::EQ_LOW), gv(P::EQ_LOWFREQ), gv(P::EQ_MID), gv(P::EQ_MIDFREQ),
                                      gv(P::EQ_MIDQ), gv(P::EQ_HIGH), gv(P::EQ_HIGHFREQ) });
                break;
            case FxId::Compressor:
                if (!was) comp.reset();
                comp.process(l, r, n, { gv(P::COMP_THRESH), gv(P::COMP_RATIO), gv(P::COMP_ATK),
                                        gv(P::COMP_REL), gv(P::COMP_MAKEUP), gv(P::COMP_MIX) });
                break;
            default: break;
        }
    }

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
    t.compGainReduction.store(fxWasOn[(int)FxId::Compressor] ? comp.grDb : 0.0f, std::memory_order_relaxed);
    t.arpNote.store(arp.numSounding > 0 ? arp.sounding[0] : -1, std::memory_order_relaxed);
    for (int o = 0; o < 2; ++o)
        t.oscPos[o].store(has ? voices[displayVoice].wtPos(o) : value[(int)oscParam(o, P::OSCA_POS)], std::memory_order_relaxed);

    t.callbacks.fetch_add(1, std::memory_order_release);
}
