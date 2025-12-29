#pragma once
#include <vector>
#include <memory>
#include "audio/FX/FX.h"


struct FXChain {
    std::vector<std::unique_ptr<FX>> chain;
    bool bypass = false;

    inline Sample process(Sample in, float sr)
    {
        if (bypass || chain.empty())
            return in;

        for (auto& fx : chain)
            in = fx->process(in, sr);

        return in;
    }

    inline void clear()
    {
        chain.clear();
    }

    inline void add(std::unique_ptr<FX> fx)
    {
        chain.push_back(std::move(fx));
    }

    void registerModTargets(ModMatrix& mod)
    {
        for (auto& fx : chain)
            fx->registerModTargets(mod);
    }

    void getAvailableModTargets(std::vector<ModTargetID>& out) const
    {
        for (auto& fx : chain)
            fx->getAvailableModTargets(out);
    }
};
