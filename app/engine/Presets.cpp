#include "engine/Presets.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

Preset::Preset()
{
    for (int i = 0; i < NUM_PARAMS; ++i) values[i] = paramInfo(i).def;
}

void Preset::mod(ModSrc src, P target, float amount, ModSrc aux, ModMode mode, ModCurve curve)
{
    ModSlotData d;
    d.src = src; d.aux = aux; d.target = (int)target; d.amount = amount; d.mode = mode; d.curve = curve;
    mods.push_back(d);
}

// frame index -> WT position
static float frame(int f) { return f / 7.0f; }

// ---------------------------------------------------------------------
// Factory sounds
// ---------------------------------------------------------------------
std::vector<Preset> factoryPresets()
{
    std::vector<Preset> out;
    auto add = [&](const char* name, const char* cat) -> Preset& {
        out.emplace_back();
        out.back().name = name;
        out.back().category = cat;
        return out.back();
    };

    { // ---- Init ----
        Preset& p = add("Init", "Init");
        p.set(P::OSCA_POS, frame(3));
    }

    { Preset& p = add("Sub Pressure", "Bass");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_OCT, -1); p.set(P::OSCA_LEVEL, 0.6f);
        p.set(P::SUB_ON, 1); p.set(P::SUB_OCT, -2); p.set(P::SUB_LEVEL, 0.7f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 380); p.set(P::FLT_RES, 0.25f); p.set(P::FLT_DRIVE, 0.2f);
        p.set(P::ENV1_ATK, 0.002f); p.set(P::ENV1_DEC, 0.4f); p.set(P::ENV1_SUS, 0.85f); p.set(P::ENV1_REL, 0.12f);
        p.set(P::ENV2_ATK, 0.0f); p.set(P::ENV2_DEC, 0.28f); p.set(P::ENV2_SUS, 0.0f); p.set(P::ENV2_REL, 0.1f);
        p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.38f);
        p.mod(ModSrc::Velocity, P::FLT_CUTOFF, 0.12f);
        p.set(P::VOICE_MODE, 2); p.set(P::GLIDE, 0.04f);
        p.set(P::DIST_ON, 1); p.set(P::DIST_MODE, 0); p.set(P::DIST_DRIVE, 0.3f); p.set(P::DIST_MIX, 0.6f);
    }

    { Preset& p = add("Reese Growl", "Bass");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_OCT, -1); p.set(P::OSCA_UNISON, 4); p.set(P::OSCA_DETUNE, 0.35f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_POS, frame(3)); p.set(P::OSCB_OCT, -1); p.set(P::OSCB_FINE, 14);
        p.set(P::OSCB_UNISON, 3); p.set(P::OSCB_DETUNE, 0.3f); p.set(P::OSCB_LEVEL, 0.6f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 900); p.set(P::FLT_RES, 0.3f);
        p.set(P::LFO1_RATE, 0.25f); p.set(P::LFO1_SHAPE, 0);
        p.mod(ModSrc::Lfo1, P::FLT_CUTOFF, 0.12f);
        p.mod(ModSrc::ModWheel, P::FLT_CUTOFF, 0.35f);
        p.set(P::ENV1_ATK, 0.004f); p.set(P::ENV1_SUS, 1.0f); p.set(P::ENV1_REL, 0.15f);
        p.set(P::VOICE_MODE, 1); p.set(P::GLIDE, 0.03f);
        p.set(P::DIST_ON, 1); p.set(P::DIST_MODE, 2); p.set(P::DIST_DRIVE, 0.4f); p.set(P::DIST_MIX, 0.7f);
    }

    { Preset& p = add("Wobble Bass", "Bass");
        p.wavetable[0] = "Analog Sweep";
        p.set(P::OSCA_POS, 0.4f); p.set(P::OSCA_OCT, -1); p.set(P::OSCA_UNISON, 2); p.set(P::OSCA_DETUNE, 0.15f);
        p.set(P::SUB_ON, 1); p.set(P::SUB_OCT, -2); p.set(P::SUB_LEVEL, 0.55f); p.set(P::SUB_FILTER, 0);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 500); p.set(P::FLT_RES, 0.5f);
        p.set(P::LFO1_SYNC, 1); p.set(P::LFO1_DIV, 9); p.set(P::LFO1_SHAPE, 1); p.set(P::LFO1_MODE, 1);
        p.mod(ModSrc::Lfo1, P::FLT_CUTOFF, 0.32f);
        p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.35f);
        p.set(P::VOICE_MODE, 2);
        p.set(P::DIST_ON, 1); p.set(P::DIST_MODE, 0); p.set(P::DIST_DRIVE, 0.5f);
    }

    { Preset& p = add("Supersaw Lead", "Lead");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_UNISON, 7); p.set(P::OSCA_DETUNE, 0.45f); p.set(P::OSCA_WIDTH, 1.0f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_POS, frame(3)); p.set(P::OSCB_OCT, 1); p.set(P::OSCB_UNISON, 5);
        p.set(P::OSCB_DETUNE, 0.35f); p.set(P::OSCB_LEVEL, 0.35f);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 6500); p.set(P::FLT_RES, 0.15f);
        p.set(P::ENV1_ATK, 0.006f); p.set(P::ENV1_SUS, 0.9f); p.set(P::ENV1_REL, 0.35f);
        p.set(P::LFO1_RATE, 5.5f); p.set(P::LFO1_RISE, 0.4f); p.set(P::LFO1_MODE, 1);
        p.mod(ModSrc::Lfo1, P::OSCA_FINE, 0.06f, ModSrc::ModWheel);
        p.mod(ModSrc::Lfo1, P::OSCB_FINE, 0.06f, ModSrc::ModWheel);
        p.set(P::GLIDE, 0.02f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_DIV, 10); p.set(P::DLY_MIX, 0.18f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.18f);
    }

    { Preset& p = add("Vowel Sync Lead", "Lead");
        p.wavetable[0] = "Vocal"; p.wavetable[1] = "Sync Sweep";
        p.set(P::OSCA_POS, 0.2f); p.set(P::OSCA_UNISON, 3); p.set(P::OSCA_DETUNE, 0.2f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_POS, 0.1f); p.set(P::OSCB_LEVEL, 0.5f);
        p.set(P::LFO2_RATE, 0.6f); p.set(P::LFO2_SHAPE, 6);
        p.mod(ModSrc::Lfo2, P::OSCA_POS, 0.45f);
        p.set(P::ENV2_ATK, 0.01f); p.set(P::ENV2_DEC, 0.8f); p.set(P::ENV2_SUS, 0.2f);
        p.mod(ModSrc::Env2, P::OSCB_POS, 0.6f);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 7000);
        p.set(P::VOICE_MODE, 2); p.set(P::GLIDE, 0.08f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_DIV, 7); p.set(P::DLY_MIX, 0.22f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.2f);
    }

    { Preset& p = add("Velocity Brass", "Lead");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_UNISON, 3); p.set(P::OSCA_DETUNE, 0.2f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_POS, frame(4)); p.set(P::OSCB_FINE, -8); p.set(P::OSCB_LEVEL, 0.45f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 700); p.set(P::FLT_RES, 0.15f);
        p.set(P::ENV2_ATK, 0.06f); p.set(P::ENV2_DEC, 0.5f); p.set(P::ENV2_SUS, 0.45f); p.set(P::ENV2_REL, 0.3f);
        p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.45f, ModSrc::Velocity);
        p.set(P::ENV1_ATK, 0.03f); p.set(P::ENV1_SUS, 0.85f); p.set(P::ENV1_REL, 0.25f);
        p.set(P::VEL_SENS, 0.8f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.2f);
    }

    { Preset& p = add("Aurora Pad", "Pad");
        p.wavetable[0] = "Harmonic Series";
        p.set(P::OSCA_POS, 0.35f); p.set(P::OSCA_UNISON, 6); p.set(P::OSCA_DETUNE, 0.3f); p.set(P::OSCA_WIDTH, 1.0f);
        p.set(P::OSCB_ON, 1); p.wavetable[1] = "Spectra"; p.set(P::OSCB_POS, 0.3f); p.set(P::OSCB_OCT, 1); p.set(P::OSCB_LEVEL, 0.3f);
        p.set(P::LFO1_RATE, 0.08f); p.set(P::LFO2_RATE, 0.13f); p.set(P::LFO2_SHAPE, 6);
        p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.35f);
        p.mod(ModSrc::Lfo2, P::OSCB_POS, 0.4f);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 3200); p.set(P::FLT_RES, 0.1f);
        p.set(P::ENV1_ATK, 1.2f); p.set(P::ENV1_DEC, 2.0f); p.set(P::ENV1_SUS, 0.85f); p.set(P::ENV1_REL, 2.8f);
        p.mod(ModSrc::ModWheel, P::FLT_CUTOFF, 0.25f);
        p.set(P::CHO_ON, 1); p.set(P::CHO_MIX, 0.35f);
        p.set(P::REV_ON, 1); p.set(P::REV_DECAY, 6.0f); p.set(P::REV_SIZE, 0.85f); p.set(P::REV_MIX, 0.42f);
    }

    { Preset& p = add("Glass Pad", "Pad");
        p.wavetable[0] = "Spectra"; p.wavetable[1] = "Vocal";
        p.set(P::OSCA_POS, 0.5f); p.set(P::OSCA_UNISON, 4); p.set(P::OSCA_DETUNE, 0.2f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_OCT, 1); p.set(P::OSCB_POS, 0.7f); p.set(P::OSCB_LEVEL, 0.3f);
        p.set(P::LFO1_SHAPE, 6); p.set(P::LFO1_RATE, 0.15f);
        p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.5f);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 5000);
        p.set(P::ENV1_ATK, 0.8f); p.set(P::ENV1_SUS, 0.9f); p.set(P::ENV1_REL, 3.0f);
        p.set(P::REV_ON, 1); p.set(P::REV_DECAY, 8.0f); p.set(P::REV_SIZE, 0.9f); p.set(P::REV_MIX, 0.5f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_DIV, 6); p.set(P::DLY_MIX, 0.15f);
    }

    { Preset& p = add("Warm Strings", "Pad");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_UNISON, 5); p.set(P::OSCA_DETUNE, 0.25f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 1800); p.set(P::FLT_RES, 0.05f); p.set(P::FLT_KEYTRK, 0.5f);
        p.set(P::ENV2_ATK, 0.5f); p.set(P::ENV2_SUS, 0.6f);
        p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.18f);
        p.set(P::ENV1_ATK, 0.55f); p.set(P::ENV1_SUS, 0.9f); p.set(P::ENV1_REL, 1.2f);
        p.set(P::CHO_ON, 1); p.set(P::CHO_MIX, 0.5f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.3f);
    }

    { Preset& p = add("Crystal Pluck", "Pluck");
        p.set(P::OSCA_POS, frame(4)); p.set(P::OSCA_UNISON, 2); p.set(P::OSCA_DETUNE, 0.12f);
        p.set(P::OSCB_ON, 1); p.set(P::OSCB_POS, frame(0)); p.set(P::OSCB_OCT, 1); p.set(P::OSCB_LEVEL, 0.4f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 450); p.set(P::FLT_RES, 0.3f);
        p.set(P::ENV2_ATK, 0.0f); p.set(P::ENV2_DEC, 0.3f); p.set(P::ENV2_SUS, 0.0f);
        p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.55f);
        p.set(P::ENV1_ATK, 0.001f); p.set(P::ENV1_DEC, 0.6f); p.set(P::ENV1_SUS, 0.0f); p.set(P::ENV1_REL, 0.45f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_DIV, 10); p.set(P::DLY_FB, 0.4f); p.set(P::DLY_MIX, 0.25f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.25f);
    }

    { Preset& p = add("Soft Keys", "Keys");
        p.wavetable[0] = "FM Bright";
        p.set(P::OSCA_POS, 0.15f);
        p.set(P::ENV2_ATK, 0.0f); p.set(P::ENV2_DEC, 0.9f); p.set(P::ENV2_SUS, 0.1f);
        p.mod(ModSrc::Env2, P::OSCA_POS, 0.4f, ModSrc::Velocity);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 5000);
        p.set(P::ENV1_ATK, 0.003f); p.set(P::ENV1_DEC, 1.6f); p.set(P::ENV1_SUS, 0.3f); p.set(P::ENV1_REL, 0.6f);
        p.set(P::VEL_SENS, 0.85f);
        p.set(P::CHO_ON, 1); p.set(P::CHO_MIX, 0.3f);
        p.set(P::REV_ON, 1); p.set(P::REV_MIX, 0.22f);
    }

    { Preset& p = add("Pulse Arp", "Pluck");
        p.wavetable[0] = "Pulse Width";
        p.set(P::OSCA_POS, 0.3f);
        p.set(P::LFO1_SHAPE, 1); p.set(P::LFO1_RATE, 0.35f);
        p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.4f);
        p.set(P::FLT_TYPE, 0); p.set(P::FLT_CUTOFF, 1500); p.set(P::FLT_RES, 0.35f);
        p.set(P::ENV2_DEC, 0.2f); p.set(P::ENV2_SUS, 0.0f);
        p.mod(ModSrc::Env2, P::FLT_CUTOFF, 0.4f);
        p.set(P::ENV1_ATK, 0.001f); p.set(P::ENV1_DEC, 0.25f); p.set(P::ENV1_SUS, 0.0f); p.set(P::ENV1_REL, 0.2f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_DIV, 9); p.set(P::DLY_FB, 0.5f); p.set(P::DLY_MIX, 0.3f);
    }

    { Preset& p = add("Noise Riser", "FX");
        p.set(P::OSCA_ON, 0);
        p.set(P::NOISE_ON, 1); p.set(P::NOISE_LEVEL, 0.7f);
        p.set(P::FLT_TYPE, 2); p.set(P::FLT_CUTOFF, 300); p.set(P::FLT_RES, 0.6f);
        p.set(P::LFO1_MODE, 2); p.set(P::LFO1_SHAPE, 2); p.set(P::LFO1_RATE, 0.125f);
        p.mod(ModSrc::Lfo1, P::FLT_CUTOFF, 0.35f);
        p.mod(ModSrc::Lfo1, P::REV_MIX, 0.25f);
        p.set(P::ENV1_ATK, 2.0f); p.set(P::ENV1_REL, 1.5f);
        p.set(P::REV_ON, 1); p.set(P::REV_DECAY, 7.0f); p.set(P::REV_MIX, 0.35f);
    }

    { Preset& p = add("Fold Drone", "FX");
        p.wavetable[0] = "Wavefold";
        p.set(P::OSCA_POS, 0.1f); p.set(P::OSCA_UNISON, 3); p.set(P::OSCA_DETUNE, 0.15f);
        p.set(P::SUB_ON, 1); p.set(P::SUB_LEVEL, 0.4f);
        p.mod(ModSrc::Macro1, P::OSCA_POS, 0.8f);
        p.set(P::LFO1_SHAPE, 6); p.set(P::LFO1_RATE, 0.2f);
        p.mod(ModSrc::Lfo1, P::OSCA_POS, 0.12f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 2500); p.set(P::FLT_RES, 0.2f);
        p.set(P::ENV1_ATK, 1.5f); p.set(P::ENV1_REL, 3.0f);
        p.set(P::REV_ON, 1); p.set(P::REV_DECAY, 9.0f); p.set(P::REV_MIX, 0.4f);
    }

    { Preset& p = add("Wah Wheel", "FX");
        p.set(P::OSCA_POS, frame(3)); p.set(P::OSCA_UNISON, 2); p.set(P::OSCA_DETUNE, 0.1f);
        p.set(P::FLT_TYPE, 1); p.set(P::FLT_CUTOFF, 350); p.set(P::FLT_RES, 0.65f);
        p.mod(ModSrc::ModWheel, P::FLT_CUTOFF, 0.6f);
        p.mod(ModSrc::Aftertouch, P::FLT_CUTOFF, 0.25f);
        p.set(P::DIST_ON, 1); p.set(P::DIST_MODE, 2); p.set(P::DIST_DRIVE, 0.45f);
        p.set(P::DLY_ON, 1); p.set(P::DLY_MIX, 0.2f);
    }

    return out;
}

// ---------------------------------------------------------------------
// Capture / apply
// ---------------------------------------------------------------------
void capturePreset(Preset& out, const SharedState& s, const ResourceManager& wt)
{
    for (int i = 0; i < NUM_PARAMS; ++i) out.values[i] = s.params.value((P)i);
    out.mods.clear();
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) {
        ModSlotData d = s.mods.get(i);
        if (d.active()) out.mods.push_back(d);
    }
    out.fxOrder = s.fxOrder.load(std::memory_order_relaxed);
    for (int l = 0; l < 4; ++l) out.lfoCurve[l] = wt.lfoCurve(l);
    for (int o = 0; o < 2; ++o) {
        const auto& w = wt.get(o);
        out.wavetable[o] = w ? w->name : "Basic Shapes";
        if (w && findFactoryWavetable(w->name) < 0) out.customFrames[o] = w->frames;
        else out.customFrames[o].clear();
    }
}

void applyPreset(const Preset& p, SharedState& s, ResourceManager& wt)
{
    for (int i = 0; i < NUM_PARAMS; ++i) s.params.setValue((P)i, p.values[i]);
    s.mods.clearAll();
    for (size_t i = 0; i < p.mods.size() && i < (size_t)NUM_MOD_SLOTS; ++i) s.mods.set((int)i, p.mods[i]);
    s.fxOrder.store(p.fxOrder, std::memory_order_relaxed);
    for (int l = 0; l < 4; ++l) wt.setLfoCurve(l, p.lfoCurve[l].size() >= 2 ? p.lfoCurve[l] : defaultLfoCurve());

    for (int o = 0; o < 2; ++o) {
        if (p.customFrames[o].size() == (size_t)WT_FRAMES * WT_SIZE) {
            auto w = std::make_shared<WavetableSet>();
            w->name = p.wavetable[o].empty() ? "Custom" : p.wavetable[o];
            w->frames = p.customFrames[o];
            w->build();
            wt.set(o, w);
        } else {
            int idx = findFactoryWavetable(p.wavetable[o]);
            wt.set(o, makeFactoryWavetable(idx < 0 ? 0 : idx));
        }
    }
}

// ---------------------------------------------------------------------
// File format (text, one setting per line)
//
//   modular_synth-preset 1
//   name <text>          category <text>
//   param <key> <value>
//   mod <src> <aux> <target-key> <amount> <mode> <curve>
//   wavetable <osc> <name>
//   frame <osc> <index> <2048 int16 as hex>
// ---------------------------------------------------------------------
static ModSrc srcFromShort(const std::string& s)
{
    for (int i = 0; i < NUM_MOD_SRC; ++i)
        if (s == modSrcShortName((ModSrc)i)) return (ModSrc)i;
    return ModSrc::None;
}

bool savePresetFile(const Preset& p, const std::string& path)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << "modular_synth-preset 1\n";
    f << "name " << p.name << "\n";
    f << "category " << p.category << "\n";
    for (int i = 0; i < NUM_PARAMS; ++i)
        f << "param " << paramInfo(i).key << " " << p.values[i] << "\n";
    {
        char hex[16];
        std::snprintf(hex, sizeof hex, "%08x", p.fxOrder);
        f << "fxorder " << hex << "\n";
    }
    for (int l = 0; l < 4; ++l) {
        if (p.lfoCurve[l].size() < 2) continue;
        f << "lfocurve " << l << " " << p.lfoCurve[l].size();
        for (const auto& pt : p.lfoCurve[l]) f << " " << pt.x << " " << pt.y << " " << pt.curve;
        f << "\n";
    }
    for (const auto& m : p.mods)
        f << "mod " << modSrcShortName(m.src) << " " << modSrcShortName(m.aux) << " "
          << paramInfo(m.target).key << " " << m.amount << " " << (int)m.mode << " " << (int)m.curve << "\n";
    for (int o = 0; o < 2; ++o) {
        f << "wavetable " << o << " " << p.wavetable[o] << "\n";
        if (p.customFrames[o].size() == (size_t)WT_FRAMES * WT_SIZE) {
            for (int fr = 0; fr < WT_FRAMES; ++fr) {
                f << "frame " << o << " " << fr << " ";
                char hex[5];
                for (int i = 0; i < WT_SIZE; ++i) {
                    float v = std::clamp(p.customFrames[o][(size_t)fr * WT_SIZE + i], -1.0f, 1.0f);
                    std::snprintf(hex, sizeof hex, "%04x", (unsigned)(uint16_t)(int16_t)std::lround(v * 32767.0f));
                    f << hex;
                }
                f << "\n";
            }
        }
    }
    return (bool)f;
}

bool loadPresetFile(Preset& p, const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::string line;
    if (!std::getline(f, line) || line.rfind("modular_synth-preset", 0) != 0) return false;

    p = Preset();
    p.name = fs::path(path).stem().string();
    p.category = "User";

    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string cmd;
        ss >> cmd;
        if (cmd == "name" || cmd == "category") {
            std::string rest;
            std::getline(ss >> std::ws, rest);
            (cmd == "name" ? p.name : p.category) = rest;
        } else if (cmd == "param") {
            std::string key; float v;
            if (ss >> key >> v) { int i = findParam(key.c_str()); if (i >= 0) p.values[i] = v; }
        } else if (cmd == "mod") {
            std::string src, aux, target; float amt = 0; int mode = 0, curve = 0;
            if (ss >> src >> aux >> target >> amt) {
                ss >> mode >> curve;
                int t = findParam(target.c_str());
                if (t >= 0) p.mod(srcFromShort(src), (P)t, amt, srcFromShort(aux),
                                  (ModMode)std::clamp(mode, 0, 1), (ModCurve)std::clamp(curve, 0, 3));
            }
        } else if (cmd == "fxorder") {
            std::string hex;
            if (ss >> hex) {
                uint32_t v = (uint32_t)std::strtoul(hex.c_str(), nullptr, 16);
                bool valid = true, seen[NUM_FX] = {};
                for (int i = 0; i < NUM_FX; ++i) {
                    int id = (int)fxAt(v, i);
                    if (id >= NUM_FX || seen[id]) valid = false; else seen[id] = true;
                }
                if (valid) p.fxOrder = v;
            }
        } else if (cmd == "lfocurve") {
            int l = -1, n = 0;
            if (ss >> l >> n && l >= 0 && l < 4 && n >= 2 && n <= 32) {
                std::vector<LfoPoint> pts;
                for (int i = 0; i < n; ++i) {
                    LfoPoint pt{};
                    if (!(ss >> pt.x >> pt.y >> pt.curve)) break;
                    pts.push_back(pt);
                }
                if ((int)pts.size() == n) p.lfoCurve[l] = pts;
            }
        } else if (cmd == "wavetable") {
            int o; std::string name;
            if (ss >> o) { std::getline(ss >> std::ws, name); if (o >= 0 && o < 2) p.wavetable[o] = name; }
        } else if (cmd == "frame") {
            int o, fr; std::string hex;
            if (ss >> o >> fr >> hex && o >= 0 && o < 2 && fr >= 0 && fr < WT_FRAMES && hex.size() >= (size_t)WT_SIZE * 4) {
                auto& frames = p.customFrames[o];
                if (frames.size() != (size_t)WT_FRAMES * WT_SIZE) frames.assign((size_t)WT_FRAMES * WT_SIZE, 0.0f);
                for (int i = 0; i < WT_SIZE; ++i) {
                    unsigned v = (unsigned)std::strtoul(hex.substr((size_t)i * 4, 4).c_str(), nullptr, 16);
                    frames[(size_t)fr * WT_SIZE + i] = (int16_t)(uint16_t)v / 32767.0f;
                }
            }
        }
    }
    return true;
}

std::string userPresetDir()
{
    const char* appdata = std::getenv("APPDATA");
    fs::path dir = fs::path(appdata ? appdata : ".") / "modular_synth" / "Presets";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

std::vector<std::string> listUserPresets()
{
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(userPresetDir(), ec))
        if (e.is_regular_file() && e.path().extension() == ".msp")
            out.push_back(e.path().string());
    std::sort(out.begin(), out.end());
    return out;
}
