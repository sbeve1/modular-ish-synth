#pragma once

#include <cstdint>
#include <cmath>
#include "control/ControlModule.h"
#include "ModSourceID.h"
#include "ModTargetID.h"

// ------------------------------------------------------------
// Serum-style modulation operations (kept from your original)
// ------------------------------------------------------------
enum class ModType {
    ADD,          // linear add
    MULTIPLY,     // linear multiply
    REPLACE,      // sets base (replace)
    EXP_CURVE,    // pow(src, amt)
    SOFT_CURVE,   // sqrt(src)
    HARD_CURVE    // src^2
};

// Stores each modulation route
struct Route {
    int srcIndex  = -1;
    int tgtIndex  = -1;
    float amount  = 0.0f;
    ModType type  = ModType::ADD;
};

// Stores accumulation for each target
struct TargetSlot {
    // pointer to *modulated shadow value* (ModMatrix writes here each process)
    float* modPtr = nullptr;

    // permanent base (knob) value captured at registration
    float base = 0.0f;

    // runtime accumulators (reset every process())
    float addAcc  = 0.0f;
    float multAcc = 1.0f;
    float replaceAcc = 0.0f;
    bool hasReplace = false;
    float curveAcc = 1.0f;
};

class ModMatrix {
public:
    static constexpr int MAX_ROUTES = 128;

    ControlModule* sources[(int)ModSourceID::COUNT];
    // note: we don't need 'targets' array of base pointers any more
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
            auto &slot = targetSlots[i];
            slot.modPtr = nullptr;
            slot.base   = 0.0f;

            slot.addAcc = 0.0f;
            slot.multAcc = 1.0f;
            slot.replaceAcc = 0.0f;
            slot.hasReplace = false;
            slot.curveAcc = 1.0f;
        }
    }

    // Register a source module (LFO, ENV, velocity, etc.)
    void registerSource(ModSourceID id, ControlModule* src)
    {
        sources[(int)id] = src;
    }

    // Register a target parameter pointer.
    // IMPORTANT: pass pointer-to-mod-shadow (where modded runtime value should be written)
    // and pass the knob/base value separately (so ModMatrix never overwrites the real knob).
    void registerTarget(ModTargetID id, float* modValuePtr, float baseValue)
    {
        auto& slot = targetSlots[(int)id];
        slot.modPtr = modValuePtr;
        slot.base   = baseValue;
    }

    // Add a modulation route to the matrix
    void addRoute(ModSourceID s, ModTargetID t, float amt, ModType type)
    {
        if (numRoutes >= MAX_ROUTES) return;
        routes[numRoutes++] = { (int)s, (int)t, amt, type };
    }

    // Process modulation every sample (or control tick)
    void process(float sr)
    {
        // 1. Read each source modulator
        for (int i = 0; i < (int)ModSourceID::COUNT; i++) {
            if (sources[i])
                modValues[i] = sources[i]->process(sr);
            else
                modValues[i] = 0.0f;
        }

        // 2. Reset accumulators for targets
        for (int i = 0; i < (int)ModTargetID::COUNT; i++) {
            auto& slot = targetSlots[i];

            slot.addAcc     = 0.0f;
            slot.multAcc    = 1.0f;
            slot.replaceAcc = 0.0f;
            slot.hasReplace = false;
            slot.curveAcc   = 1.0f;
        }

        // 3. Apply all modulation routes into accumulators
        for (int r = 0; r < numRoutes; r++)
        {
            const Route& rt = routes[r];
            float srcVal = modValues[rt.srcIndex];
            auto& slot = targetSlots[rt.tgtIndex];

            switch (rt.type)
            {
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
                {
                    float v = srcVal;
                    if (v < 0.0001f) v = 0.0001f;
                    slot.curveAcc *= powf(v, rt.amount);
                }
                break;

                case ModType::SOFT_CURVE:
                {
                    float v = srcVal;
                    if (v < 0.0f) v = 0.0f;
                    slot.curveAcc *= sqrtf(v);
                }
                break;

                case ModType::HARD_CURVE:
                {
                    float v = srcVal;
                    if (v < 0.0f) v = 0.0f;
                    slot.curveAcc *= (v * v);
                }
                break;
            }
        }

        // 4. Write final runtime-modulated values to the mod-shadows
        for (int i = 0; i < (int)ModTargetID::COUNT; i++)
        {
            auto& slot = targetSlots[i];
            if (!slot.modPtr) continue;

            // start from the permanent base (knob) value
            float baseVal = slot.base;

            // Serum order: REPLACE → ADD → MULTIPLY → CURVE
            float out;
            if (slot.hasReplace)
                out = slot.replaceAcc;
            else
                out = baseVal + slot.addAcc;

            out *= slot.multAcc;
            out *= slot.curveAcc;

            *slot.modPtr = out; // write only to the mod-shadow!
        }
    }
};

