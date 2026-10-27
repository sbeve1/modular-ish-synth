#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "engine/Params.h"

// =====================================================================
// State shared between the UI/MIDI threads and the audio thread.
// Everything is lock-free: SPSC rings for events, atomics for values.
// =====================================================================

// ---- single-producer single-consumer ring ----
template <typename T, int N>
class SpscRing {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
public:
    bool push(const T& v)
    {
        int h = head.load(std::memory_order_relaxed);
        int next = (h + 1) & (N - 1);
        if (next == tail.load(std::memory_order_acquire)) return false;
        buf[h] = v;
        head.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& out)
    {
        int t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) return false;
        out = buf[t];
        tail.store((t + 1) & (N - 1), std::memory_order_release);
        return true;
    }
private:
    T buf[N];
    std::atomic<int> head{ 0 }, tail{ 0 };
};

// ---- note / controller events ----
enum class EventType : uint8_t { NoteOn, NoteOff, ControlChange, PitchBend, Aftertouch, AllNotesOff };

struct Event {
    EventType type;
    uint8_t a = 0;      // note or CC number
    uint8_t b = 0;      // velocity or CC value (0..127)
    float   f = 0.0f;   // pitch bend -1..1, aftertouch 0..1
};

using EventQueue = SpscRing<Event, 1024>;

// ---- modulation ----
enum class ModSrc : int {
    None, Env1, Env2, Env3, Lfo1, Lfo2, Lfo3, Lfo4,
    Velocity, Note, ModWheel, Aftertouch, PitchBend, Random,
    Macro1, Macro2, Macro3, Macro4,
    COUNT
};
constexpr int NUM_MOD_SRC = (int)ModSrc::COUNT;
const char* modSrcName(ModSrc s);
const char* modSrcShortName(ModSrc s);
bool modSrcBipolar(ModSrc s);

constexpr int NUM_MOD_SLOTS = 32;

// Carried over from the original ModMatrix's ModType: how a slot combines
// with the knob position, and how the source value is shaped first.
enum class ModMode  : int { Add, Multiply };              // Multiply scales the knob position
enum class ModCurve : int { Linear, Exp, Soft, Hard };    // x, x^3, sqrt(x), x^2 (sign kept)
const char* modModeName(ModMode m);
const char* modCurveName(ModCurve c);

struct ModSlotData {
    ModSrc src = ModSrc::None;
    ModSrc aux = ModSrc::None;   // optional scaler (e.g. mod wheel controls depth)
    int target = -1;             // param index
    float amount = 0.0f;         // -1..1 of the target's knob range
    ModMode mode = ModMode::Add;
    ModCurve curve = ModCurve::Linear;
    bool active() const { return src != ModSrc::None && target >= 0; }
};

inline float shapeModValue(float v, ModCurve c)
{
    float a = v < 0.0f ? -v : v, s = v < 0.0f ? -1.0f : 1.0f;
    switch (c) {
        case ModCurve::Exp:  return s * a * a * a;
        case ModCurve::Soft: return s * std::sqrt(a);
        case ModCurve::Hard: return s * a * a;
        default:             return v;
    }
}

class ModSlotStore {
public:
    // Per-slot seqlock: the (single) writer makes the version odd while it
    // writes; readers retry until they see the same even version before and
    // after, so a slot is never read half-updated.
    ModSlotData get(int i) const
    {
        ModSlotData d;
        for (;;) {
            uint32_t v1 = ver[i].load(std::memory_order_acquire);
            if (v1 & 1u) continue;
            d.src    = (ModSrc)src[i].load(std::memory_order_relaxed);
            d.aux    = (ModSrc)aux[i].load(std::memory_order_relaxed);
            d.target = target[i].load(std::memory_order_relaxed);
            d.amount = amount[i].load(std::memory_order_relaxed);
            d.mode   = (ModMode)mode[i].load(std::memory_order_relaxed);
            d.curve  = (ModCurve)curve[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (ver[i].load(std::memory_order_relaxed) == v1) return d;
        }
    }
    void set(int i, const ModSlotData& d)
    {
        uint32_t v = ver[i].load(std::memory_order_relaxed);
        ver[i].store(v + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        src[i].store((int)d.src, std::memory_order_relaxed);
        aux[i].store((int)d.aux, std::memory_order_relaxed);
        target[i].store(d.target, std::memory_order_relaxed);
        amount[i].store(d.amount, std::memory_order_relaxed);
        mode[i].store((int)d.mode, std::memory_order_relaxed);
        curve[i].store((int)d.curve, std::memory_order_relaxed);
        ver[i].store(v + 2, std::memory_order_release);
        gen.fetch_add(1, std::memory_order_relaxed);
    }
    void setAmount(int i, float a)
    {
        amount[i].store(a, std::memory_order_relaxed);   // a single field: no seqlock needed
        gen.fetch_add(1, std::memory_order_relaxed);
    }
    uint32_t generation() const { return gen.load(std::memory_order_relaxed); }
    void clear(int i) { set(i, ModSlotData{}); }
    void clearAll() { for (int i = 0; i < NUM_MOD_SLOTS; ++i) clear(i); }

    int firstFree() const
    {
        for (int i = 0; i < NUM_MOD_SLOTS; ++i)
            if (!get(i).active()) return i;
        return -1;
    }

private:
    std::atomic<int>   src[NUM_MOD_SLOTS]{};
    std::atomic<int>   aux[NUM_MOD_SLOTS]{};
    std::atomic<int>   target[NUM_MOD_SLOTS]{};
    std::atomic<float> amount[NUM_MOD_SLOTS]{};
    std::atomic<int>   mode[NUM_MOD_SLOTS]{};
    std::atomic<int>   curve[NUM_MOD_SLOTS]{};
    std::atomic<uint32_t> ver[NUM_MOD_SLOTS]{};
    std::atomic<uint32_t> gen{ 0 };   // bumped on every change (lets the UI skip work)

public:
    ModSlotStore() { clearAll(); }
};

// ---- audio -> UI telemetry (visuals only; never read back by audio) ----
struct Telemetry {
    static constexpr int SCOPE_SIZE = 8192;

    // recent output, mono, written continuously (UI copies a window)
    float scope[SCOPE_SIZE] = {};
    std::atomic<uint32_t> scopeWrite{ 0 };

    std::atomic<float> peakL{ 0 }, peakR{ 0 };     // per block peak, UI decays
    std::atomic<float> cpuLoad{ 0 };              // callback time / buffer time
    std::atomic<int>   activeVoices{ 0 };
    std::atomic<uint32_t> notes[4]{};             // 128-bit held-note mask
    std::atomic<uint64_t> callbacks{ 0 };          // epoch for retiring wavetables

    // most recently triggered voice (for live modulation displays)
    std::atomic<int>   displayVoiceActive{ 0 };
    std::atomic<float> srcValue[NUM_MOD_SRC]{};
    std::atomic<float> modNorm[NUM_PARAMS]{};       // effective normalized value
    // range each modulated param swept since the UI last looked (fast LFOs move
    // further between frames than a single sampled value can show)
    std::atomic<float> modMin[NUM_PARAMS]{}, modMax[NUM_PARAMS]{};
    std::atomic<uint32_t> modRangeRead{ 0 };         // UI bumps this after reading the ranges
    std::atomic<float> lfoPhase[4]{};
    std::atomic<int>   envStage[3]{};                // 0 idle, 1 A, 2 H, 3 D, 4 S, 5 R
    std::atomic<float> envStageTime[3]{};
    std::atomic<float> oscPos[2]{};                  // modulated WT position
    std::atomic<float> compGainReduction{ 0 };       // dB, for the compressor meter
    std::atomic<int>   arpNote{ -1 };                // note the arpeggiator is playing

    bool noteHeld(int n) const { return (notes[n >> 5].load(std::memory_order_relaxed) >> (n & 31)) & 1u; }
};
