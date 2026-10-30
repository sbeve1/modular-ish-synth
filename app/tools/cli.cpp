// modular_synth_cli: headless rendering, benchmark and self-test for the engine.
//
//   modular_synth_cli render <preset-number|name> out.wav
//   modular_synth_cli bench
//   modular_synth_cli selftest
//   modular_synth_cli list

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "engine/Instrument.h"
#include "engine/Presets.h"
#include "engine/ResourceManager.h"
#include "platform/miniaudio.h"

// ---- allocation counter (for the zero-allocation test) ----
static std::atomic<bool> gCountAllocs{ false };
static std::atomic<long> gAllocs{ 0 };

void* operator new(size_t n)
{
    if (gCountAllocs.load(std::memory_order_relaxed)) gAllocs.fetch_add(1, std::memory_order_relaxed);
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, size_t) noexcept { std::free(p); }

// ---- offline rendering ----
struct NoteEv { double t; int note; int vel; };   // vel 0 = off

static std::vector<NoteEv> demoSequence()
{
    std::vector<NoteEv> ev;
    auto play = [&](double t, double len, int n, int v) { ev.push_back({ t, n, v }); ev.push_back({ t + len, n, 0 }); };
    const int prog[4][4] = { { 45, 57, 60, 64 }, { 41, 53, 57, 60 }, { 48, 55, 60, 64 }, { 43, 55, 59, 62 } };
    for (int bar = 0; bar < 4; ++bar) {
        double t0 = bar * 2.0;
        for (int k = 0; k < 4; ++k) play(t0, 1.8, prog[bar][k], k == 0 ? 110 : 85);
        for (int s = 0; s < 8; ++s) play(t0 + s * 0.25, 0.2, prog[bar][1 + (s % 3)] + 12, 70 + (s % 4) * 12);
    }
    std::sort(ev.begin(), ev.end(), [](const NoteEv& a, const NoteEv& b) {
        return a.t != b.t ? a.t < b.t : (a.vel == 0 && b.vel != 0);
    });
    return ev;
}

struct Engine {
    std::unique_ptr<SharedState> shared = std::make_unique<SharedState>();
    std::unique_ptr<Instrument>  inst;
    std::unique_ptr<ResourceManager> wt;
    float sr;

    explicit Engine(float sampleRate = 48000.0f) : sr(sampleRate)
    {
        inst = std::make_unique<Instrument>(*shared);
        wt = std::make_unique<ResourceManager>(*shared);
        inst->prepare(sr, 1024);
    }

    void load(const Preset& p) { applyPreset(p, *shared, *wt); }

    // Renders the events; events are applied at chunk boundaries (<= 64 samples).
    std::vector<float> render(const std::vector<NoteEv>& ev, double tail)
    {
        double end = (ev.empty() ? 0.0 : ev.back().t) + tail;
        size_t total = (size_t)(end * sr);
        std::vector<float> L(total), R(total), out(total * 2);
        size_t pos = 0, next = 0;
        while (pos < total) {
            while (next < ev.size() && (size_t)(ev[next].t * sr) <= pos) {
                const NoteEv& e = ev[next++];
                Event x{ e.vel ? EventType::NoteOn : EventType::NoteOff, (uint8_t)e.note, (uint8_t)e.vel };
                shared->uiEvents.push(x);
            }
            size_t until = next < ev.size() ? (size_t)(ev[next].t * sr) : total;
            size_t n = std::min<size_t>({ total - pos, std::max<size_t>(until - pos, 1), 64 });
            inst->process(&L[pos], &R[pos], (int)n);
            pos += n;
        }
        for (size_t i = 0; i < total; ++i) { out[2 * i] = L[i]; out[2 * i + 1] = R[i]; }
        return out;
    }
};

static bool writeWav(const char* path, const std::vector<float>& inter, unsigned sr)
{
    ma_encoder_config cfg = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, sr);
    ma_encoder enc;
    if (ma_encoder_init_file(path, &cfg, &enc) != MA_SUCCESS) return false;
    ma_uint64 frames = inter.size() / 2, written = 0;
    ma_result r = ma_encoder_write_pcm_frames(&enc, inter.data(), frames, &written);
    ma_encoder_uninit(&enc);
    return r == MA_SUCCESS && written == frames;
}

static const Preset* findPreset(const std::vector<Preset>& all, const std::string& key)
{
    char* end = nullptr;
    long idx = std::strtol(key.c_str(), &end, 10);
    if (end && *end == 0 && idx >= 1 && idx <= (long)all.size()) return &all[idx - 1];
    for (const auto& p : all) if (p.name == key) return &p;
    return nullptr;
}

// ---------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------
static int cmdList()
{
    auto all = factoryPresets();
    for (size_t i = 0; i < all.size(); ++i)
        std::printf("%2zu  %-8s %s\n", i + 1, all[i].category.c_str(), all[i].name.c_str());
    return 0;
}

static int cmdRender(const std::string& key, const char* path)
{
    auto all = factoryPresets();
    const Preset* p = findPreset(all, key);
    if (!p) { std::printf("Unknown preset '%s'\n", key.c_str()); return 1; }

    Engine e;
    e.load(*p);
    auto t0 = std::chrono::steady_clock::now();
    auto audio = e.render(demoSequence(), 4.0);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    float peak = 0.0f;
    for (float v : audio) peak = std::max(peak, std::fabs(v));
    if (!writeWav(path, audio, (unsigned)e.sr)) { std::printf("Failed to write %s\n", path); return 1; }
    double len = audio.size() / 2 / e.sr;
    std::printf("%s: %s, %.1fs, peak %.3f, %.1fx realtime\n", path, p->name.c_str(), len, peak, len / secs);
    return 0;
}

// Seconds of CPU to render `seconds` of audio with `voices` held notes (best of 3).
static double timeRender(const Preset& p, int voices, double seconds)
{
    double best = 1e9;
    for (int run = 0; run < 3; ++run) {
        std::vector<NoteEv> ev;
        for (int v = 0; v < voices; ++v) ev.push_back({ 0.0, 40 + v * 3, 100 });
        ev.push_back({ seconds, 0, 0 });
        Engine eng;
        eng.load(p);
        auto t0 = std::chrono::steady_clock::now();
        eng.render(ev, 0.0);
        best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
    return best;
}

static Preset benchHeavy()
{
    Preset p;
    p.name = "Heavy";
    p.set(P::OSCA_UNISON, 8); p.set(P::OSCB_ON, 1); p.set(P::OSCB_UNISON, 8);
    p.set(P::SUB_ON, 1); p.set(P::NOISE_ON, 1); p.set(P::FLT_TYPE, 1); p.set(P::POLYPHONY, 16);
    p.set(P::ENV1_SUS, 1.0f);
    p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.5f);
    p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.4f);
    p.set(P::REV_ON, 1); p.set(P::DLY_ON, 1); p.set(P::CHO_ON, 1); p.set(P::DIST_ON, 1);
    return p;
}

static Preset benchMax()
{
    Preset p = benchHeavy();
    p.name = "Max";
    p.set(P::OSCA_UNISON, 16); p.set(P::OSCB_UNISON, 16);
    p.set(P::OSCA_WARP, 1); p.set(P::OSCA_WARPAMT, 0.8f);   // Bend
    p.set(P::OSCB_WARP, 3); p.set(P::OSCB_WARPAMT, 0.4f);   // Sync
    p.set(P::FLT2_ON, 1); p.set(P::FLT2_TYPE, 5); p.set(P::FLT_ROUTING, 1);
    p.mod(ModSrc::Lfo2, P::OSCB_POS, 0.3f);
    p.mod(ModSrc::Env3, P::FLT2_CUTOFF, 0.3f);
    p.mod(ModSrc::Velocity, P::OSCA_LEVEL, 0.2f);
    p.mod(ModSrc::Lfo1, P::OSCA_WARPAMT, 0.2f);
    for (int i = 0; i < NUM_FX; ++i) p.set(fxPowerParam((FxId)i), 1);
    return p;
}

static Preset benchFxOnly()
{
    Preset p;
    p.name = "FX rack only";
    p.wavetable[0] = "Basic Shapes";
    for (int i = 0; i < NUM_FX; ++i) p.set(fxPowerParam((FxId)i), 1);
    p.set(P::ENV1_SUS, 1.0f);
    return p;
}

static int cmdBench()
{
    const double seconds = 10.0;
    std::printf("Engine benchmark, 48000 Hz, best of 3 runs of %.0f s each\n\n", seconds);

    Preset heavy = benchHeavy();
    std::printf("Heavy patch: 2 osc x 8 unison + sub + noise + 24 dB filter, 4 FX\n");
    std::printf("%8s %14s %14s\n", "voices", "x realtime", "% of a core");
    for (int voices : { 1, 4, 8, 16 }) {
        double cpu = timeRender(heavy, voices, seconds);
        std::printf("%8d %13.1fx %13.2f%%\n", voices, seconds / cpu, cpu / seconds * 100.0);
    }

    std::printf("\n%-44s %14s %14s\n", "scenario (16 voices unless noted)", "x realtime", "% of a core");
    struct S { Preset p; int voices; const char* label; };
    S scen[] = {
        { benchMax(), 16, "Max: 2x16 unison, bend+sync warp, 2 filters, all 8 FX" },
        { benchFxOnly(), 1, "FX rack only: 1 plain voice, all 8 FX" },
    };
    for (auto& sc : scen) {
        double cpu = timeRender(sc.p, sc.voices, seconds);
        std::printf("%-44s %13.1fx %13.2f%%\n", sc.label, seconds / cpu, cpu / seconds * 100.0);
    }

    // where the Max scenario's time goes: remove one part at a time
    std::printf("\nMax breakdown (16 voices), cost with one part removed:\n");
    struct V { const char* label; void (*apply)(Preset&); };
    const V parts[] = {
        { "no warp",       [](Preset& p) { p.set(P::OSCA_WARP, 0); p.set(P::OSCB_WARP, 0); } },
        { "bend off",      [](Preset& p) { p.set(P::OSCA_WARP, 0); } },
        { "sync off",      [](Preset& p) { p.set(P::OSCB_WARP, 0); } },
        { "unison 1",      [](Preset& p) { p.set(P::OSCA_UNISON, 1); p.set(P::OSCB_UNISON, 1); } },
        { "filters off",   [](Preset& p) { p.set(P::FLT_ON, 0); p.set(P::FLT2_ON, 0); } },
        { "no modulation", [](Preset& p) { p.mods.clear(); } },
        { "FX off",        [](Preset& p) { for (int i = 0; i < NUM_FX; ++i) p.set(fxPowerParam((FxId)i), 0); } },
    };
    for (const auto& v : parts) {
        Preset p = benchMax();
        v.apply(p);
        double cpu = timeRender(p, 16, seconds);
        std::printf("  %-16s %8.2f%%\n", v.label, cpu / seconds * 100.0);
    }
    return 0;
}

// Per-preset level fingerprint, to check optimizations don't change the sound.
static int cmdFingerprint()
{
    auto all = factoryPresets();
    all.push_back(benchMax());
    for (const auto& p : all) {
        Engine e;
        e.load(p);
        auto audio = e.render(demoSequence(), 3.0);
        double sum = 0.0;
        float peak = 0.0f;
        for (float v : audio) { sum += (double)v * v; peak = std::max(peak, std::fabs(v)); }
        std::printf("%-18s rms %.6f  peak %.6f\n", p.name.c_str(), std::sqrt(sum / audio.size()), peak);
    }
    return 0;
}

// ---- showcase: a short multi-preset demo for the README ----
// Each segment renders one preset with its own notes; segments overlap so
// release tails blend into the next part. Output is normalized to -1 dBFS
// and written as 16-bit PCM to keep the file small.
static int cmdShowcase(const char* path)
{
    struct Segment {
        const char* preset;
        double start;                 // seconds into the demo
        std::vector<NoteEv> notes;    // times relative to start
        double tail;
        void (*tweak)(Preset&);
        float gain;                   // level balance between parts
    };
    auto chord = [](std::initializer_list<int> ns, double t, double len, int vel) {
        std::vector<NoteEv> v;
        for (int n : ns) { v.push_back({ t, n, vel }); v.push_back({ t + len, n, 0 }); }
        return v;
    };
    auto join = [](std::vector<NoteEv> a, const std::vector<NoteEv>& b) { a.insert(a.end(), b.begin(), b.end()); return a; };

    // pad: Am -> F -> C -> G
    std::vector<NoteEv> pad = join(join(chord({ 57, 60, 64, 69 }, 0.0, 2.9, 90), chord({ 53, 57, 60, 65 }, 3.0, 2.9, 90)),
                                   join(chord({ 55, 60, 64, 67 }, 6.0, 2.9, 90), chord({ 55, 59, 62, 67 }, 9.0, 2.9, 90)));
    // lead melody over the second half of the pad
    std::vector<NoteEv> lead;
    const double mel[][3] = { { 0.0, 76, 0.45 }, { 0.5, 74, 0.45 }, { 1.0, 72, 0.9 }, { 2.0, 74, 0.45 }, { 2.5, 76, 0.45 },
                              { 3.0, 79, 1.4 }, { 4.5, 76, 0.45 }, { 5.0, 74, 0.9 } };
    for (auto& m : mel) { lead.push_back({ m[0], (int)m[1], 105 }); lead.push_back({ m[0] + m[2], (int)m[1], 0 }); }
    // bassline, eighths at 120 BPM
    std::vector<NoteEv> bass;
    const int line[16] = { 33, 33, 45, 33, 36, 36, 48, 36, 29, 29, 41, 29, 31, 31, 43, 31 };
    for (int i = 0; i < 16; ++i) { bass.push_back({ i * 0.25, line[i], i % 2 ? 90 : 120 }); bass.push_back({ i * 0.25 + 0.2, line[i], 0 }); }

    std::vector<Segment> segs = {
        { "Aurora Pad",    0.0,  pad,  3.0, nullptr, 1.35f },
        { "Supersaw Lead", 6.0,  lead, 2.0, nullptr, 1.0f },
        { "Sub Pressure",  12.5, bass, 1.0, nullptr, 0.95f },
        { "Pulse Arp",     16.5, chord({ 57, 60, 64, 67 }, 0.0, 4.0, 100), 2.0,
          [](Preset& p) { p.set(P::ARP_ON, 1); p.set(P::ARP_MODE, 2); p.set(P::ARP_DIV, 12); p.set(P::ARP_OCT, 2); }, 3.2f },
        { "Glass Pad",     20.5, chord({ 45, 57, 64, 71, 76 }, 0.0, 3.5, 85), 5.0, nullptr, 0.7f },
    };

    const float sr = 48000.0f;
    double total = 0.0;
    for (auto& s : segs) total = std::max(total, s.start + (s.notes.empty() ? 0.0 : s.notes.back().t) + s.tail + 4.0);
    std::vector<float> mix((size_t)(total * sr) * 2, 0.0f);
    size_t used = 0;

    auto all = factoryPresets();
    for (auto& s : segs) {
        const Preset* base = findPreset(all, s.preset);
        if (!base) { std::printf("missing preset %s\n", s.preset); return 1; }
        Preset p = *base;
        if (s.tweak) s.tweak(p);
        std::sort(s.notes.begin(), s.notes.end(), [](const NoteEv& a, const NoteEv& b) {
            return a.t != b.t ? a.t < b.t : (a.vel == 0 && b.vel != 0);
        });
        Engine e(sr);
        e.load(p);
        auto audio = e.render(s.notes, s.tail);
        size_t off = (size_t)(s.start * sr) * 2;
        for (size_t i = 0; i < audio.size() && off + i < mix.size(); ++i) mix[off + i] += audio[i] * s.gain;
        used = std::max(used, std::min(off + audio.size(), mix.size()));
        std::printf("  %-14s at %5.1fs\n", s.preset, s.start);
    }
    mix.resize(used);

    float peak = 0.0f;
    for (float v : mix) peak = std::max(peak, std::fabs(v));
    float g = peak > 0.0f ? 0.891f / peak : 1.0f;   // -1 dBFS
    // short fade-out on the last 0.5 s
    size_t fade = (size_t)(0.5f * sr) * 2;
    for (size_t i = 0; i < mix.size(); ++i) {
        float f = i + fade > mix.size() ? (float)(mix.size() - i) / fade : 1.0f;
        mix[i] *= g * f;
    }

    std::vector<int16_t> pcm(mix.size());
    for (size_t i = 0; i < mix.size(); ++i) pcm[i] = (int16_t)std::lround(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f);
    ma_encoder_config cfg = ma_encoder_config_init(ma_encoding_format_wav, ma_format_s16, 2, (ma_uint32)sr);
    ma_encoder enc;
    if (ma_encoder_init_file(path, &cfg, &enc) != MA_SUCCESS) { std::printf("Failed to write %s\n", path); return 1; }
    ma_uint64 written = 0;
    ma_encoder_write_pcm_frames(&enc, pcm.data(), pcm.size() / 2, &written);
    ma_encoder_uninit(&enc);
    std::printf("Wrote %s: %.1f s, 16-bit stereo, normalized from peak %.3f\n", path, pcm.size() / 2 / sr, peak);
    return 0;
}

// ---- lfotest: does each LFO shape/rate actually move its target? ----
// Renders a held note with LFO 1 -> Osc A level at full depth and measures how
// much the short-window level swings (0 = no modulation, 1 = full on/off).
static double modulationSwing(const Preset& p, double seconds)
{
    Engine e;
    e.load(p);
    std::vector<NoteEv> ev = { { 0.0, 60, 100 }, { seconds, 60, 0 } };
    auto audio = e.render(ev, 0.0);
    const size_t win = (size_t)(0.001 * e.sr);   // 1 ms windows
    std::vector<double> lv;
    for (size_t s = (size_t)(0.1 * e.sr); s + win < audio.size() / 2; s += win) {   // skip the attack
        double acc = 0.0;
        for (size_t i = 0; i < win; ++i) { float v = audio[2 * (s + i)]; acc += v * v; }
        lv.push_back(std::sqrt(acc / win));
    }
    std::sort(lv.begin(), lv.end());
    double lo = lv[lv.size() / 20], hi = lv[lv.size() * 19 / 20];   // 5th / 95th percentile
    return hi > 1e-9 ? (hi - lo) / hi : 0.0;
}

static int cmdLfoTest()
{
    const char* shapes[] = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Drift", "Custom" };
    const float rates[] = { 2.0f, 10.0f, 40.0f };
    std::printf("LFO 1 -> Osc A level (amount 100%%): level swing, 0 = none, 1 = full\n");
    std::printf("%-10s %8s %8s %8s\n", "shape", "2 Hz", "10 Hz", "40 Hz");
    for (int sh = 0; sh < 8; ++sh) {
        std::printf("%-10s", shapes[sh]);
        for (float r : rates) {
            Preset p;
            p.set(P::OSCA_POS, 0.0f);            // sine wave: steady level without modulation
            p.set(P::OSCA_LEVEL, 0.5f);
            p.set(P::ENV1_SUS, 1.0f);
            p.set(P::FLT_ON, 0);
            p.set(P::LFO1_SHAPE, (float)sh);
            p.set(P::LFO1_RATE, r);
            p.mod(ModSrc::Lfo1, P::OSCA_LEVEL, 1.0f);
            std::printf(" %8.2f", modulationSwing(p, 1.0));
        }
        std::printf("\n");
    }
    return 0;
}

// ---- filter check: resonance vs passband, drive vs distortion ----
// Level of one frequency in the left channel over [t0, t1) seconds (Goertzel), in dB.
static float toneDb(const std::vector<float>& inter, float sr, float freq, double t0, double t1)
{
    size_t a = (size_t)(t0 * sr), b = std::min((size_t)(t1 * sr), inter.size() / 2);
    double w = 2.0 * 3.14159265358979 * freq / sr, c = 2.0 * std::cos(w), s1 = 0.0, s2 = 0.0;
    for (size_t i = a; i < b; ++i) { double s0 = inter[2 * i] + c * s1 - s2; s2 = s1; s1 = s0; }
    double power = s1 * s1 + s2 * s2 - c * s1 * s2;
    double amp = 2.0 * std::sqrt(std::max(power, 0.0)) / (double)(b - a);
    return (float)(20.0 * std::log10(std::max(amp, 1e-9)));
}

static std::vector<float> renderHeldNote(const Preset& p, int note, double len)
{
    Engine e;
    e.load(p);
    return e.render({ { 0.0, note, 100 }, { len, note, 0 } }, 0.05);
}

static int cmdFilterTest()
{
    const float sr = 48000.0f;
    std::printf("Resonance: saw at 65.4 Hz into the filter, cutoff 1.5 kHz\n");
    std::printf("%-6s %5s %16s %16s\n", "type", "res", "passband (dB)", "at cutoff (dB)");
    for (int type : { 0, 1 }) {
        for (float res : { 0.0f, 0.5f, 1.0f }) {
            Preset p;
            p.set(P::OSCA_POS, 3.0f / 7.0f);   // Basic Shapes frame 3: plain saw
            p.set(P::OSCA_LEVEL, 0.25f);
            p.set(P::ENV1_SUS, 1.0f);
            p.set(P::FLT_ON, 1); p.set(P::FLT_TYPE, (float)type);
            p.set(P::FLT_CUTOFF, 1500.0f); p.set(P::FLT_RES, res); p.set(P::FLT_DRIVE, 0.0f);
            auto a = renderHeldNote(p, 36, 1.0);
            std::printf("%-6s %5.2f %16.1f %16.1f\n", type ? "LP 24" : "LP 12", res,
                        toneDb(a, sr, 65.406f, 0.5, 1.0), toneDb(a, sr, 65.406f * 23, 0.5, 1.0));
        }
    }

    std::printf("\nDrive: sine at 220 Hz through LP 12 (cutoff 20 kHz, res 0)\n");
    std::printf("%5s %12s %10s\n", "drive", "level (dB)", "THD (%)");
    for (float drive : { 0.0f, 0.25f, 0.5f, 1.0f }) {
        Preset p;
        p.set(P::OSCA_POS, 0.0f);   // sine
        p.set(P::OSCA_LEVEL, 0.25f);
        p.set(P::ENV1_SUS, 1.0f);
        p.set(P::FLT_ON, 1); p.set(P::FLT_TYPE, 0.0f);
        p.set(P::FLT_CUTOFF, 20000.0f); p.set(P::FLT_RES, 0.0f); p.set(P::FLT_DRIVE, drive);
        auto a = renderHeldNote(p, 57, 1.0);
        float f0 = toneDb(a, sr, 220.0f, 0.5, 1.0);
        double harm = 0.0;
        for (int h = 2; h <= 10; ++h) harm += std::pow(10.0, toneDb(a, sr, 220.0f * h, 0.5, 1.0) / 10.0);
        double thd = 100.0 * std::sqrt(harm) / std::pow(10.0, f0 / 20.0);
        std::printf("%5.2f %12.1f %10.2f\n", drive, f0, thd);
    }
    return 0;
}

// ---- self-test ----
static int gFails = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf("  pass  "); } else { std::printf("  FAIL  "); ++gFails; } std::printf(__VA_ARGS__); std::printf("\n"); } while (0)

static int cmdSelftest()
{
    std::printf("Parameters\n");
    {
        int bad = 0;
        for (int i = 0; i < NUM_PARAMS; ++i) {
            const ParamInfo& p = paramInfo(i);
            for (float n : { 0.0f, 0.25f, 0.5f, 1.0f }) {
                float v = p.toValue(n), back = p.toValue(p.toNorm(v));
                if (std::fabs(back - v) > 1e-3f * std::max(1.0f, std::fabs(v))) ++bad;
            }
            if (std::fabs(p.toValue(p.toNorm(p.def)) - p.def) > 1e-3f * std::max(1.0f, std::fabs(p.def))) ++bad;
            if (findParam(p.key) != i) ++bad;
        }
        CHECK(bad == 0, "%d params round-trip value <-> normalized, keys unique (%d errors)", NUM_PARAMS, bad);
    }

    std::printf("Factory presets render cleanly\n");
    auto all = factoryPresets();
    for (const auto& p : all) {
        Engine e;
        e.load(p);
        auto audio = e.render(demoSequence(), 10.0);
        bool finite = true;
        float peak = 0.0f, tailPeak = 0.0f;
        size_t tailStart = audio.size() - (size_t)(0.5 * e.sr) * 2;
        for (size_t i = 0; i < audio.size(); ++i) {
            if (!std::isfinite(audio[i])) finite = false;
            peak = std::max(peak, std::fabs(audio[i]));
            if (i >= tailStart) tailPeak = std::max(tailPeak, std::fabs(audio[i]));
        }
        int voicesLeft = e.shared->telemetry.activeVoices.load();
        CHECK(finite && peak > 0.01f && peak <= 1.0f && voicesLeft == 0,
              "%-16s peak %.3f, tail %.5f, voices left %d", p.name.c_str(), peak, tailPeak, voicesLeft);
    }

    std::printf("Real-time safety\n");
    {
        Engine e;
        Preset p = all[4];   // supersaw: unison, FX
        for (int i = 0; i < NUM_FX; ++i) p.set(fxPowerParam((FxId)i), 1);
        p.set(P::FLT2_ON, 1); p.set(P::FLT_ROUTING, 1);
        p.set(P::ARP_ON, 1); p.set(P::ARP_MODE, 3);
        p.set(P::LFO1_SHAPE, 7);
        p.fxOrder = 0;
        for (int i = 0; i < NUM_FX; ++i) p.fxOrder |= (uint32_t)(NUM_FX - 1 - i) << (i * 4);   // reversed
        e.load(p);
        std::vector<float> L(512), R(512);
        e.inst->process(L.data(), R.data(), 512);   // warm-up
        gAllocs = 0;
        gCountAllocs = true;
        for (int b = 0; b < 400; ++b) {
            if (b % 20 == 0) e.shared->uiEvents.push({ EventType::NoteOn, (uint8_t)(48 + b / 20), 100 });
            if (b % 20 == 10) e.shared->uiEvents.push({ EventType::NoteOff, (uint8_t)(48 + b / 20), 0 });
            e.shared->params.setNorm(P::FLT_CUTOFF, (b % 50) / 50.0f);
            e.inst->process(L.data(), R.data(), 512);
        }
        gCountAllocs = false;
        CHECK(gAllocs.load() == 0, "no heap allocations in 400 audio blocks: notes, arp, both filters, all 8 FX reordered (%ld)", gAllocs.load());
    }

    std::printf("Modulation\n");
    {
        auto rms = [](const std::vector<float>& a) { double s = 0; for (float v : a) s += v * v; return std::sqrt(s / a.size()); };
        std::vector<NoteEv> one = { { 0.0, 60, 100 }, { 1.0, 60, 0 } };
        Engine a, b;
        Preset p;
        a.load(p);
        p.set(P::MACRO1, 1.0f);
        p.mod(ModSrc::Macro1, P::OSCA_LEVEL, -1.0f);
        b.load(p);
        double ra = rms(a.render(one, 0.2)), rb = rms(b.render(one, 0.2));
        CHECK(ra > 0.01 && rb < ra * 0.01, "Macro 1 -> Osc A level -100%% silences the oscillator (%.4f -> %.6f)", ra, rb);

        Engine c, d;
        Preset q;
        q.set(P::FLT_CUTOFF, 200.0f);
        c.load(q);
        q.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.8f);
        d.load(q);
        double rc = rms(c.render(one, 0.2)), rd = rms(d.render(one, 0.2));
        CHECK(rd > rc * 1.5, "Env 2 -> cutoff opens the filter (%.4f -> %.4f)", rc, rd);
    }

    std::printf("Arpeggiator\n");
    {
        Engine e;
        Preset p;
        p.set(P::ARP_ON, 1); p.set(P::ARP_MODE, 0); p.set(P::ARP_DIV, 12); p.set(P::TEMPO, 120);
        e.load(p);
        std::vector<float> L(64), R(64);
        e.inst->process(L.data(), R.data(), 64);
        for (int n : { 67, 60, 64 }) e.shared->uiEvents.push({ EventType::NoteOn, (uint8_t)n, 100 });
        std::vector<int> seq;
        int last = -1;
        size_t samples = 0;
        while (samples < (size_t)(e.sr * 1.0)) {           // 1 s = 8 sixteenths at 120 BPM
            e.inst->process(L.data(), R.data(), 64);
            samples += 64;
            int n = e.shared->telemetry.arpNote.load();
            if (n >= 0 && n != last) seq.push_back(n);
            last = n;
        }
        bool order = seq.size() >= 6;
        const int want[6] = { 60, 64, 67, 60, 64, 67 };
        for (int i = 0; i < 6 && order; ++i) order = seq[i] == want[i];
        CHECK(order && seq.size() >= 7 && seq.size() <= 9, "Up mode at 1/16, 120 BPM: %zu steps in 1 s, order 60 64 67 60 64 67", seq.size());
    }

    std::printf("Filter 2\n");
    {
        auto rms = [](const std::vector<float>& a) { double s = 0; for (float v : a) s += v * v; return std::sqrt(s / a.size()); };
        std::vector<NoteEv> one = { { 0.0, 48, 100 }, { 0.5, 48, 0 } };
        Preset p;
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 400.0f);          // LP 24 at 400 Hz
        p.set(P::FLT2_TYPE, 5); p.set(P::FLT2_CUTOFF, 4000.0f);       // HP 24 at 4 kHz
        Engine a; a.load(p);
        p.set(P::FLT2_ON, 1);
        Engine serial; serial.load(p);
        p.set(P::FLT_ROUTING, 1);
        Engine parallel; parallel.load(p);
        double ra = rms(a.render(one, 0.1)), rs = rms(serial.render(one, 0.1)), rp = rms(parallel.render(one, 0.1));
        CHECK(rs < ra * 0.1 && rp > ra * 0.5, "LP 400 Hz -> HP 4 kHz in series cuts (%.4f -> %.4f), in parallel passes (%.4f)", ra, rs, rp);
    }

    std::printf("Filter resonance and drive\n");
    {
        auto passband = [&](float res) {
            Preset p;
            p.set(P::OSCA_POS, 3.0f / 7.0f); p.set(P::OSCA_LEVEL, 0.25f); p.set(P::ENV1_SUS, 1.0f);
            p.set(P::FLT_ON, 1); p.set(P::FLT_TYPE, 0.0f); p.set(P::FLT_CUTOFF, 1500.0f); p.set(P::FLT_RES, res);
            return toneDb(renderHeldNote(p, 36, 1.0), 48000.0f, 65.406f, 0.5, 1.0);
        };
        float p0 = passband(0.0f), p1 = passband(1.0f);
        CHECK(p1 < p0 - 6.0f, "LP 12 passband sinks as resonance rises (%.1f dB at res 0 -> %.1f dB at res 1)", p0, p1);

        auto drive = [&](float d, float& thd) {
            Preset p;
            p.set(P::OSCA_POS, 0.0f); p.set(P::OSCA_LEVEL, 0.25f); p.set(P::ENV1_SUS, 1.0f);
            p.set(P::FLT_ON, 1); p.set(P::FLT_TYPE, 0.0f); p.set(P::FLT_CUTOFF, 20000.0f); p.set(P::FLT_DRIVE, d);
            auto a = renderHeldNote(p, 57, 1.0);
            float f0 = toneDb(a, 48000.0f, 220.0f, 0.5, 1.0);
            double harm = 0.0;
            for (int h = 2; h <= 10; ++h) harm += std::pow(10.0, toneDb(a, 48000.0f, 220.0f * h, 0.5, 1.0) / 10.0);
            thd = (float)(100.0 * std::sqrt(harm) / std::pow(10.0, f0 / 20.0));
            return f0;
        };
        float t0, t5, t1;
        float l0 = drive(0.0f, t0), l1 = drive(1.0f, t1);
        drive(0.5f, t5);
        CHECK(t0 < 1.0f && t5 > t0 * 2.0f && t1 > t5 * 3.0f && l1 > l0 + 10.0f,
              "drive raises the level into the ceiling and saturates (THD %.1f%% -> %.1f%% -> %.1f%%, level %+.1f dB)",
              t0, t5, t1, l1 - l0);
    }

    std::printf("Custom LFO\n");
    {
        auto rms = [](const std::vector<float>& a) { double s = 0; for (float v : a) s += v * v; return std::sqrt(s / a.size()); };
        std::vector<NoteEv> one = { { 0.0, 60, 100 }, { 0.5, 60, 0 } };
        Preset p;
        p.set(P::LFO1_SHAPE, 7);
        p.mod(ModSrc::Lfo1, P::OSCA_LEVEL, 1.0f);
        p.lfoCurve[0] = { { 0.0f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };   // flat at -1: level -> 0
        Engine down; down.load(p);
        p.lfoCurve[0] = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };     // flat at 0: unchanged
        Engine flat; flat.load(p);
        double rd = rms(down.render(one, 0.1)), rf = rms(flat.render(one, 0.1));
        CHECK(rf > 0.01 && rd < rf * 0.01, "drawn curve drives modulation (flat 0: %.4f, flat -1: %.6f)", rf, rd);
    }

    std::printf("LFO display matches the sound\n");
    {
        // the value the engine uses vs the value the graph draws at the reported phase
        int worstShape = -1;
        double worst = 1.0;
        for (int sh = 0; sh < 8; ++sh) {
            for (float offset : { 0.0f, 0.3f }) {
                Engine e;
                Preset p;
                p.set(P::LFO1_SHAPE, (float)sh);
                p.set(P::LFO1_RATE, 3.0f);
                p.set(P::LFO1_PHASE, offset);
                p.set(P::ENV1_SUS, 1.0f);
                e.load(p);
                std::vector<float> L(64), R(64);
                e.inst->process(L.data(), R.data(), 64);
                e.shared->uiEvents.push({ EventType::NoteOn, 60, 100 });
                int good = 0, total = 0;
                auto curve = defaultLfoCurve();
                for (int b = 0; b < 1500; ++b) {   // 2 s in 64-sample buffers
                    e.inst->process(L.data(), R.data(), 64);
                    const Telemetry& t = e.shared->telemetry;
                    float ph = t.lfoPhase[0].load(), v = t.srcValue[(int)ModSrc::Lfo1].load();
                    float pv = t.lfoPrev[0].load(), nx = t.lfoNext[0].load();
                    float drawn;
                    auto shape = (LfoShape)sh;
                    if (shape == LfoShape::SampleHold) drawn = pv;
                    else if (shape == LfoShape::Drift) { float u = ph * ph * (3 - 2 * ph); drawn = pv + (nx - pv) * u; }
                    else if (shape == LfoShape::Custom) drawn = evalLfoCurve(curve, ph);
                    else drawn = lfoShapePreview(shape, ph);
                    ++total;
                    if (std::fabs(drawn - v) < 0.05f) ++good;
                }
                double frac = (double)good / total;
                if (frac < worst) { worst = frac; worstShape = sh; }
            }
        }
        CHECK(worst > 0.97, "all 8 shapes, with and without phase offset: value matches the drawn curve %.1f%% of the time (worst: shape %d)",
              worst * 100.0, worstShape);
    }

    std::printf("Preset files\n");
    {
        Preset p = all[5];
        p.customFrames[0].assign((size_t)WT_FRAMES * WT_SIZE, 0.0f);
        for (size_t i = 0; i < p.customFrames[0].size(); ++i) p.customFrames[0][i] = std::sin(i * 0.01f);
        p.wavetable[0] = "My Table";
        p.lfoCurve[2] = { { 0.0f, 0.5f, 0.2f }, { 0.4f, -0.8f, -0.3f }, { 1.0f, 0.1f, 0.0f } };
        p.fxOrder = DEFAULT_FX_ORDER;
        std::swap(((uint8_t*)&p.fxOrder)[0], ((uint8_t*)&p.fxOrder)[3]);
        std::string path = (std::filesystem::temp_directory_path() / "modular_synth_selftest.msp").string();
        bool saved = savePresetFile(p, path);
        Preset q;
        bool loaded = loadPresetFile(q, path);
        float maxDiff = 0.0f, maxWt = 0.0f;
        for (int i = 0; i < NUM_PARAMS; ++i) maxDiff = std::max(maxDiff, std::fabs(p.values[i] - q.values[i]));
        for (size_t i = 0; i < q.customFrames[0].size(); ++i) maxWt = std::max(maxWt, std::fabs(p.customFrames[0][i] - q.customFrames[0][i]));
        CHECK(saved && loaded && q.name == p.name && q.mods.size() == p.mods.size() && maxDiff < 1e-4f &&
              q.customFrames[0].size() == p.customFrames[0].size() && maxWt < 1e-4f && q.wavetable[0] == "My Table" &&
              q.fxOrder == p.fxOrder && q.lfoCurve[2].size() == 3 && std::fabs(q.lfoCurve[2][1].y + 0.8f) < 1e-4f,
              "save/load round-trip: params, %zu mod slots, custom wavetable, LFO curve, FX order", p.mods.size());
        std::filesystem::remove(path);
    }

    std::printf("Lock-free queue\n");
    {
        auto q = std::make_unique<EventQueue>();
        const int N = 2000000;
        std::atomic<bool> ok{ true };
        std::thread prod([&] {
            for (int i = 0; i < N; ++i) {
                Event e{ EventType::NoteOn, (uint8_t)(i & 127), (uint8_t)((i >> 7) & 127), (float)i };
                while (!q->push(e)) std::this_thread::yield();
            }
        });
        int expected = 0;
        while (expected < N) {
            Event e;
            if (q->pop(e)) {
                if ((int)e.f != expected || e.a != (expected & 127)) ok = false;
                ++expected;
            }
        }
        prod.join();
        CHECK(ok.load(), "2M events across two threads arrive complete and in order");
    }

    std::printf("\n%s (%d failure%s)\n", gFails ? "FAILED" : "ALL PASSED", gFails, gFails == 1 ? "" : "s");
    return gFails ? 1 : 0;
}

int main(int argc, char** argv)
{
    std::string cmd = argc > 1 ? argv[1] : "";
    if (cmd == "list") return cmdList();
    if (cmd == "render" && argc > 3) return cmdRender(argv[2], argv[3]);
    if (cmd == "bench") return cmdBench();
    if (cmd == "fingerprint") return cmdFingerprint();
    if (cmd == "lfotest") return cmdLfoTest();
    if (cmd == "filtertest") return cmdFilterTest();
    if (cmd == "showcase" && argc > 2) return cmdShowcase(argv[2]);
    if (cmd == "selftest") return cmdSelftest();

    std::printf("Usage:\n  modular_synth_cli list\n  modular_synth_cli render <preset-number|name> out.wav\n"
                "  modular_synth_cli bench\n  modular_synth_cli fingerprint\n  modular_synth_cli selftest\n");
    return 1;
}
