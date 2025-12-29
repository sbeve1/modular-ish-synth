#pragma once
#include "core/Sample.h"
#include "engine/ModMatrix.h"
#include "engine/ModTargetID.h"
#include <vector>
#include <algorithm>


struct FX {

    // =========================
    // FX-slot controls
    // =========================
    float wetBase = 1.0f;   // 1 = fully wet by default
    float wetMod  = 0.0f;   // for modulation later
    bool  bypass  = false;

    virtual ~FX() = default;

    // Subclasses implement this (100% wet)
    virtual Sample processWet(const Sample& in, float sr) = 0;

    // Final wrapper (do NOT override)
    Sample process(const Sample& in, float sr)
    {
        if (bypass)
            return in;

        float wet = std::clamp(wetBase + wetMod, 0.0f, 1.0f);
        Sample w = processWet(in, sr);

        Sample out{};
        out.left  = in.left  * (1.0f - wet) + w.left  * wet;
        out.right = in.right * (1.0f - wet) + w.right * wet;
        return out;
    }

    virtual void registerModTargets(ModMatrix& mod)
    {
        mod.registerTarget(
            ModTargetID::FX_WET,
            &wetMod,
            wetBase,
            0.0f, 1.0f
        );
    }

    struct ModTargetInfo {
        ModTargetID id;
    };

    virtual void getAvailableModTargets(std::vector<ModTargetID>& out) const {}

};
