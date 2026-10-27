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

static int cmdBench()
{
    Engine e;
    Preset p;
    p.name = "Bench";
    p.set(P::OSCA_UNISON, 8); p.set(P::OSCB_ON, 1); p.set(P::OSCB_UNISON, 8);
    p.set(P::SUB_ON, 1); p.set(P::NOISE_ON, 1); p.set(P::FLT_TYPE, 1); p.set(P::POLYPHONY, 16);
    p.set(P::ENV1_SUS, 1.0f);
    p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.5f);
    p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.4f);
    p.set(P::REV_ON, 1); p.set(P::DLY_ON, 1); p.set(P::CHO_ON, 1); p.set(P::DIST_ON, 1);
    e.load(p);

    const double seconds = 10.0;
    std::printf("Engine benchmark: 2 osc x 8 unison + sub + noise + 24 dB filter per voice, all FX on, %d Hz\n", (int)e.sr);
    std::printf("%8s %14s %14s\n", "voices", "x realtime", "us/voice/blk");
    for (int voices : { 1, 4, 8, 16 }) {
        std::vector<NoteEv> ev;
        for (int v = 0; v < voices; ++v) ev.push_back({ 0.0, 48 + v * 3, 100 });
        ev.push_back({ seconds, 0, 0 });
        Engine eng;
        eng.load(p);
        auto t0 = std::chrono::steady_clock::now();
        eng.render(ev, 0.0);
        double cpu = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        double blocks = seconds * eng.sr / 256.0;
        std::printf("%8d %13.1fx %14.2f\n", voices, seconds / cpu, cpu / blocks / voices * 1e6);
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
    if (cmd == "selftest") return cmdSelftest();

    std::printf("Usage:\n  modular_synth_cli list\n  modular_synth_cli render <preset-number|name> out.wav\n"
                "  modular_synth_cli bench\n  modular_synth_cli selftest\n");
    return 1;
}
