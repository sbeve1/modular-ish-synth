#pragma once

#include <vector>
#include <memory>

#include "audio/FX/FX.h"
#include "engine/ModMatrix.h"
#include "engine/ModTargetID.h"

// Simple FX chain where each FX receives the SAME input (send semantics)
// and returns wet-only audio. The chain sums (adds) all wet outputs.
struct FXChain {
    std::vector<std::unique_ptr<FX>> chain;
    bool bypass = false;

    // Process: returns accumulated WET-only Sample from all chain elements.
    Sample process(const Sample& in, float sr)
    {
        if (bypass)
            return Sample{};

        Sample wetAccum{};
        for (auto& fx : chain) {
            if (!fx) continue;
            Sample w = fx->process(in, sr); // fx returns wet-only
            wetAccum.left  += w.left;
            wetAccum.right += w.right;
        }
        return wetAccum;
    }

    inline void clear()
    {
        chain.clear();
    }

    inline void add(std::unique_ptr<FX> fx)
    {
        chain.push_back(std::move(fx));
    }

    // Allow registering mod targets for the whole chain
    void registerModTargets(ModMatrix& mod)
    {
        for (auto& fx : chain) {
            if (fx) fx->registerModTargets(mod);
        }
    }

    void getAvailableModTargets(std::vector<ModTargetID>& out) const
    {
        for (auto& fx : chain) {
            if (fx) fx->getAvailableModTargets(out);
        }
    }
};
