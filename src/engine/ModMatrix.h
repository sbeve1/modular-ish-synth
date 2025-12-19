#pragma once
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "audio/ControlModule.h"

// ModMatrix: Serum-style realtime-safe modulation matrix.
// - No allocations in process()
// - Supports Add, Multiply, Replace
// - Use addRoute(...) from the non-audio thread (UI/voice start/stop).
// - process(...) is audio-thread safe as long as addRoute/removeRoutes are not
//   called concurrently. If you need dynamic changes during audio, use double-buffering.

// CONTROLModMatrix might be a better name. no audio patching here

enum class ModType {
    ADD,
    MULTIPLY,
    REPLACE
};

struct ModRoute {
    int modIndex;     // index into modulators vector
    int targetIndex;  // index into targets vector
    float amount;
    ModType type;
};

struct TargetSlot {
    float* ptr = nullptr;   // pointer to target variable
    float base = 0.0f;      // base value captured when the target was first registered
    // accumulators used in process()
    float addAcc = 0.0f;
    float replaceAcc = 0.0f;
    float multAcc = 1.0f;
    bool hasReplace = false;
};

class ModMatrix {
public:
    ModMatrix() = default;

    // Add a modulation route.
    // Should be called off the audio thread (e.g. during voice start or UI).
    void addRoute(ControlModule* src, float* target, float amount, ModType type = ModType::ADD) {
        if (!src || !target) return;

        // get or create modulator index
        int modIndex = -1;
        auto keyM = modIndexMap.find(src);
        if (keyM == modIndexMap.end()) {
            modIndex = static_cast<int>(modulators.size());
            modulators.push_back(src);
            modIndexMap[src] = modIndex;
        } else {
            modIndex = keyM->second;
        }

        // get or create target index
        int targetIndex = -1;
        auto keyT = targetIndexMap.find(target);
        if (keyT == targetIndexMap.end()) {
            targetIndex = static_cast<int>(targets.size());
            TargetSlot ts;
            ts.ptr = target;
            ts.base = *target; // capture base at registration time
            targets.push_back(ts);
            targetIndexMap[target] = targetIndex;
        } else {
            targetIndex = keyT->second;
        }

        // push route
        ModRoute r;
        r.modIndex = modIndex;
        r.targetIndex = targetIndex;
        r.amount = amount;
        r.type = type;
        routes.push_back(r);
    }

    // Remove all routes that write to this target pointer (call off audio thread)
    void removeRoutesForTarget(float* target) {
        if (!target) return;
        auto it = targetIndexMap.find(target);
        if (it == targetIndexMap.end()) return;

        int tIdx = it->second;
        // remove routes referencing the target
        routes.erase(std::remove_if(routes.begin(), routes.end(),
                    [tIdx](const ModRoute& r){ return r.targetIndex == tIdx; }),
                    routes.end());

        // remove target slot and update maps/indices
        // to keep process() allocation-free we compact vectors and fix indices
        targets.erase(targets.begin() + tIdx);

        // rebuild targetIndexMap (cheap, called off audio thread)
        targetIndexMap.clear();
        for (int i = 0; i < (int)targets.size(); ++i)
            targetIndexMap[targets[i].ptr] = i;

        // fix route.targetIndex values
        for (auto &r : routes) {
            if (r.targetIndex > tIdx) --r.targetIndex;
        }
    }

    // Update the recorded base value for a target (useful when you want an explicit base).
    // Call off the audio thread.
    void updateBaseValue(float* target, float newBase) {
        auto it = targetIndexMap.find(target);
        if (it == targetIndexMap.end()) return;
        targets[it->second].base = newBase;
    }

    // Clears everything. Call off audio thread.
    void clear() {
        modulators.clear();
        modIndexMap.clear();
        modValues.clear();
        targets.clear();
        targetIndexMap.clear();
        routes.clear();
    }

    // Audio-thread safe process: no allocations, only vector reads/writes.
    // process modulators -> modValues
    // accumulate the targets using modValues and based on ur MODE, set that parameter to that
    void process(float sampleRate) {
        if (routes.empty()) return;

        // 1) Compute each modulator value exactly once
        // Ensure modValues vector matches modulators size
        modValues.assign(modulators.size(), 0.0f);
        for (size_t i = 0; i < modulators.size(); i++) {
            // call control-rate processing (returns float modulation value)
            modValues[i] = modulators[i]->process(sampleRate);
        }

        // 2) Reset accumulators in targets (fast, contiguous)
        for (size_t t = 0; t < targets.size(); t++) {
            targets[t].addAcc = 0.0f;
            targets[t].replaceAcc = 0.0f;
            targets[t].multAcc = 1.0f;
            targets[t].hasReplace = false;
        }

        // 3) Accumulate each route
        for (const ModRoute &r : routes) {
            float srcVal = modValues[r.modIndex];
            TargetSlot &slot = targets[r.targetIndex];

            switch (r.type) {
                case ModType::ADD:
                    slot.addAcc += srcVal * r.amount;
                    break;
                case ModType::MULTIPLY:
                    // multiplicative modulation applied as multiplier (1 + src*amt)
                    // so amount = how strongly the source influences multiplier
                    slot.multAcc *= (1.0f + srcVal * r.amount);
                    break;
                case ModType::REPLACE:
                    slot.replaceAcc = srcVal * r.amount;
                    slot.hasReplace = true;
                    break;
            }
        }

        // 4) Commit results to target pointers (write final values)
        for (size_t t = 0; t < targets.size(); ++t) {
            TargetSlot &slot = targets[t];
            float finalVal;
            if (slot.hasReplace) {
                // Replace semantics: replace takes precedence
                finalVal = slot.replaceAcc;
            } else {
                finalVal = (slot.base + slot.addAcc) * slot.multAcc;
            }
            *slot.ptr = finalVal;
        }
    }

private:
    // at the same index i, these all refer to the same module. same with the target ones
    std::vector<ControlModule*> modulators;
    std::unordered_map<ControlModule*, int> modIndexMap; // used only when add/removing a modulator (not in process)
    std::vector<float> modValues; // sized at process() time, read-only during process

    // targets
    std::vector<TargetSlot> targets;
    std::unordered_map<float*, int> targetIndexMap; // used only when add/remove a target (not in process)

    // routes can use the same module. MAX: 1 route = 1 module -> 1 target
    std::vector<ModRoute> routes;
};
