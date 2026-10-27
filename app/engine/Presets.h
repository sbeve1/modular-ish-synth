#pragma once

#include <string>
#include <vector>

#include "engine/Params.h"
#include "engine/Shared.h"
#include "engine/WavetableManager.h"

// A complete sound: every parameter (plain values), the mod matrix, and
// both oscillators' wavetables (factory name, or custom frames).
struct Preset {
    std::string name = "Init";
    std::string category = "Init";
    float values[NUM_PARAMS];
    std::vector<ModSlotData> mods;
    std::string wavetable[2] = { "Basic Shapes", "Basic Shapes" };
    std::vector<float> customFrames[2];   // WT_FRAMES * WT_SIZE, or empty

    Preset();
    void set(P p, float v) { values[(int)p] = v; }
    void mod(ModSrc src, P target, float amount, ModSrc aux = ModSrc::None,
             ModMode mode = ModMode::Add, ModCurve curve = ModCurve::Linear);
};

std::vector<Preset> factoryPresets();

void capturePreset(Preset& out, const SharedState& s, const WavetableManager& wt);
void applyPreset(const Preset& p, SharedState& s, WavetableManager& wt);

bool savePresetFile(const Preset& p, const std::string& path);
bool loadPresetFile(Preset& p, const std::string& path);

// Folder for user presets (%APPDATA%\Halcyon\Presets), created on demand.
std::string userPresetDir();
std::vector<std::string> listUserPresets();   // full paths, sorted
