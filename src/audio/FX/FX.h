#pragma once

#include "core/Sample.h"
#include "engine/ModMatrix.h"
#include "engine/ModTargetID.h"

#include <vector>
#include <algorithm>

// Base class for all FX. Design contract:
//  - Derived classes implement processWet(...) and return 100% wet signal.
//  - FX::process(...) applies per-slot wet amount and bypass and returns WET-ONLY.
//  - Engine / FXChain is responsible for mixing wet returns back into the dry signal.
class FX {
public:
    FX() = default;
    virtual ~FX() = default;

    // Public settings (can be modulated via ModMatrix)
    bool bypass = false;     // global bypass for the whole unit
    float wetBase = 1.0f;    // base wet amount (0.0 .. 1.0)
    float wetMod  = 0.0f;    // modulated wet delta (added to wetBase when computing wet)

    // Subclasses must implement this and return 100% wet output based on the input.
    virtual Sample processWet(const Sample& in, float sr) = 0;

    // Top-level process used by engine/FXChain.
    // Returns WET-ONLY (scaled by wet amount). Does not mix dry in.
    Sample process(const Sample& in, float sr)
    {
        if (bypass) {
            return Sample{}; // silence = zero wet
        }

        const float wet = std::clamp(wetBase + wetMod, 0.0f, 1.0f);
        Sample w = processWet(in, sr);
        w.left  *= wet;
        w.right *= wet;
        return w;
    }

    // Register modulatable targets with ModMatrix (default registers wet)
    virtual void registerModTargets(ModMatrix& mod)
    {
        mod.registerTarget(
            ModTargetID::FX_WET,
            &wetMod,
            wetBase,
            0.0f, 1.0f
        );
    }

    // Provide a list of extra available mod targets (optional for subclasses)
    virtual void getAvailableModTargets(std::vector<ModTargetID>& out) const
    {
        // default: nothing extra
    }
};
