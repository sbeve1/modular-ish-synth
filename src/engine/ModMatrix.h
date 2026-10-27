#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include "control/ControlModule.h"
#include "ModSourceID.h"
#include "ModTargetID.h"

enum class ModType {
    ADD,
    MULTIPLY,
    REPLACE,
    EXP_CURVE,
    SOFT_CURVE,
    HARD_CURVE
};

struct Route {
    int srcIndex  = -1;
    int tgtIndex  = -1;
    float amount  = 0.0f;
    ModType type  = ModType::ADD;
};

struct TargetSlot {
    float* modPtr = nullptr;
    float base = 0.0f;

    float addAcc  = 0.0f;
    float multAcc = 1.0f;
    float replaceAcc = 0.0f;
    bool hasReplace = false;
    float curveAcc = 1.0f;
    // Target limits (clamped when writing out)
    float minVal = -std::numeric_limits<float>::infinity();
    float maxVal =  std::numeric_limits<float>::infinity();
};

class ModMatrix {
public:
    static constexpr int MAX_ROUTES = 128;

    ControlModule* sources[(int)ModSourceID::COUNT];
    float modValues[(int)ModSourceID::COUNT];
    TargetSlot targetSlots[(int)ModTargetID::COUNT];

    Route routes[MAX_ROUTES];
    int numRoutes = 0;

    ModMatrix() { clearAll(); }

    void clearAll()
    {
        numRoutes = 0;

        for (int i = 0; i < (int)ModSourceID::COUNT; i++) {
            sources[i]   = nullptr;
            modValues[i] = 0.0f;
        }

        for (int i = 0; i < (int)ModTargetID::COUNT; i++) {
            targetSlots[i] = {};
        }
    }

    void registerSource(ModSourceID id, ControlModule* src)
    {
        sources[(int)id] = src;
    }

    // Register a mod target. minVal/maxVal are optional and default to +/-inf
    void registerTarget(ModTargetID id,
                        float* modValuePtr,
                        float baseValue,
                        float minVal = -std::numeric_limits<float>::infinity(),
                        float maxVal =  std::numeric_limits<float>::infinity())
    {
        auto& slot = targetSlots[(int)id];
        slot.modPtr = modValuePtr;
        slot.base   = baseValue;
        slot.minVal = minVal;
        slot.maxVal = maxVal;
    }

    void addRoute(ModSourceID s, ModTargetID t, float amt, ModType type)
    {
        if (numRoutes >= MAX_ROUTES) return;
        routes[numRoutes++] = { (int)s, (int)t, amt, type };
    }

    void process(float /*sr*/)
    {
        // 1. READ control values (do NOT advance)
        for (int i = 0; i < (int)ModSourceID::COUNT; i++) {
            modValues[i] = sources[i] ? sources[i]->getValue() : 0.0f;
        }

        // 2. Reset accumulators
        for (int i = 0; i < (int)ModTargetID::COUNT; i++) {
            auto& slot = targetSlots[i];
            slot.addAcc = 0.0f;
            slot.multAcc = 1.0f;
            slot.replaceAcc = 0.0f;
            slot.hasReplace = false;
            slot.curveAcc = 1.0f;
        }

        // 3. Accumulate routes
        for (int r = 0; r < numRoutes; r++) {
            const Route& rt = routes[r];
            float srcVal = modValues[rt.srcIndex];
            auto& slot = targetSlots[rt.tgtIndex];

            switch (rt.type) {
                case ModType::ADD:
                    slot.addAcc += srcVal * rt.amount;
                    break;
                case ModType::MULTIPLY:
                    slot.multAcc *= (1.0f + srcVal * rt.amount);
                    break;
                case ModType::REPLACE:
                    slot.replaceAcc = srcVal * rt.amount;
                    slot.hasReplace = true;
                    break;
                case ModType::EXP_CURVE:
                    slot.curveAcc *= powf(std::max(srcVal, 0.0001f), rt.amount);
                    break;
                case ModType::SOFT_CURVE:
                    slot.curveAcc *= sqrtf(std::max(srcVal, 0.0f));
                    break;
                case ModType::HARD_CURVE:
                    slot.curveAcc *= std::max(srcVal, 0.0f) * std::max(srcVal, 0.0f);
                    break;
            }
        }

        // 4. Write results
        for (int i = 0; i < (int)ModTargetID::COUNT; i++) {
            auto& slot = targetSlots[i];
            if (!slot.modPtr) continue;

            float out = slot.base;

            if (slot.hasReplace)
                out = slot.replaceAcc;
            else
                out += slot.addAcc;

            out *= slot.multAcc;
            out *= slot.curveAcc;

            out = std::clamp(out, slot.minVal, slot.maxVal);

            // modPtr holds an offset: consumers compute base + mod
            *slot.modPtr = out - slot.base;
        }
    }
};
