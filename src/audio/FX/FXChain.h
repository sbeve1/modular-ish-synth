#pragma once
#include <vector>
#include <memory>
#include "audio/fx/FX.h"

struct FXChain {
    std::vector<std::unique_ptr<FX>> chain;
    float wet = 1.0f;   // global wet/dry mix for chain
    bool bypass = false;

    inline Sample process(Sample s, float sr) {
        if (bypass || chain.empty()) {
            // wet=0 → passthrough; wet=1 → full processed
            if (wet >= 1.0f) return s;
            // dry/wet mix (no processing)
            Sample out;
            out.left  = s.left  * (1.0f - wet);
            out.right = s.right * (1.0f - wet);
            return out;
        }

        Sample processed = s;
        for (auto& fxptr : chain) {
            if (fxptr)
                processed = fxptr->process(processed, sr);
        }

        // mix processed and dry using wet parameter
        Sample out;
        out.left  = processed.left  * wet + s.left  * (1.0f - wet);
        out.right = processed.right * wet + s.right * (1.0f - wet);
        return out;
    }

    inline void clear() { chain.clear(); }

    // take ownership
    inline void add(std::unique_ptr<FX> fx) {
        chain.push_back(std::move(fx));
    }
};
