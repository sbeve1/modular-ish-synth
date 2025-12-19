#pragma once

#include <cstdint>
#include <cstring>
#include "control/ControlModule.h"
#include "ModSourceID.h"
#include "ModTargetID.h"

enum class ModType {
    ADD,
    MULTIPLY,
    REPLACE
};

struct Route {
    int srcIndex  = -1;
    int tgtIndex  = -1;
    float amount  = 0.0f;
    ModType type  = ModType::ADD;
};

struct TargetSlot {
    float* ptr = nullptr;
    float base = 0.0f;

    float addAcc = 0.0f;
    float replaceAcc = 0.0f;
    float multAcc = 1.0f;
    bool hasReplace = false;
};

class ModMatrix {
public:
    static constexpr int MAX_ROUTES = 128;

    ControlModule* sources[(int)ModSourceID::COUNT];
    float*         targets[(int)ModTargetID::COUNT];

    float modValues[(int)ModSourceID::COUNT];
    TargetSlot targetSlots[(int)ModTargetID::COUNT];

    Route routes[MAX_ROUTES];
    int numRoutes = 0;

    ModMatrix() { clearAll(); }

    void clearAll()
    {
        numRoutes = 0;

        // zero source values and null pointers
        for (int i = 0; i < (int)ModSourceID::COUNT; i++)
        {
            sources[i]   = nullptr;
            modValues[i] = 0.0f;
        }

        for (int i = 0; i < (int)ModTargetID::COUNT; i++)
        {
            targets[i] = nullptr;
            targetSlots[i].ptr = nullptr;
            targetSlots[i].base = 0.0f;
        }
    }

    // called by Voice during note-on
    void registerSource(ModSourceID id, ControlModule* src)
    {
        sources[(int)id] = src;
    }

    void registerTarget(ModTargetID id, float* p)
    {
        targets[(int)id] = p;
        targetSlots[(int)id].ptr = p;
        targetSlots[(int)id].base = *p;
    }

    void addRoute(ModSourceID s, ModTargetID t, float amt, ModType type)
    {
        if (numRoutes >= MAX_ROUTES) return;
        routes[numRoutes++] = {
            (int)s, (int)t, amt, type
        };
    }

    // called every sample
    void process(float sr)
    {
        // compute source values
        for (int i = 0; i < (int)ModSourceID::COUNT; i++)
        {
            if (sources[i])
                modValues[i] = sources[i]->process(sr);
            else
                modValues[i] = 0.0f;
        }

        // reset target accumulators
        for (int i = 0; i < (int)ModTargetID::COUNT; i++)
        {
            auto& slot = targetSlots[i];
            slot.addAcc = 0.0f;
            slot.replaceAcc = 0.0f;
            slot.multAcc = 1.0f;
            slot.hasReplace = false;
        }

        // apply all routes
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
            }
        }

        // write final values
        for (int i = 0; i < (int)ModTargetID::COUNT; i++)
        {
            auto& slot = targetSlots[i];
            if (slot.ptr)
            {
                float out = slot.hasReplace
                    ? slot.replaceAcc
                    : (slot.base + slot.addAcc) * slot.multAcc;

                *slot.ptr = out;
            }
        }
    }
};
