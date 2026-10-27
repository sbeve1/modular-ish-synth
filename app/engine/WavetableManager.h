#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "engine/Instrument.h"

// Owns the wavetables the audio thread reads (UI thread only).
//
// set() publishes a new table with an atomic store. The old table is kept
// alive until the audio callback counter has advanced twice: the callback
// loads the pointer once per block, so after two more callbacks nothing
// can still be reading it.
class WavetableManager {
public:
    explicit WavetableManager(SharedState& s) : shared(s) {}

    void set(int osc, std::shared_ptr<WavetableSet> wt)
    {
        if (current[osc])
            retired.push_back({ current[osc], shared.telemetry.callbacks.load(std::memory_order_acquire) });
        current[osc] = std::move(wt);
        shared.wavetables[osc].store(current[osc].get(), std::memory_order_release);
    }

    const std::shared_ptr<WavetableSet>& get(int osc) const { return current[osc]; }

    // Call regularly (every UI frame). With no audio running, callbacks never
    // advance, so pass audioRunning=false to free immediately.
    void collect(bool audioRunning = true)
    {
        uint64_t now = shared.telemetry.callbacks.load(std::memory_order_acquire);
        for (size_t i = 0; i < retired.size();) {
            if (!audioRunning || now >= retired[i].epoch + 2) {
                retired[i] = retired.back();
                retired.pop_back();
            } else {
                ++i;
            }
        }
    }

private:
    struct Retired { std::shared_ptr<WavetableSet> wt; uint64_t epoch; };
    SharedState& shared;
    std::shared_ptr<WavetableSet> current[2];
    std::vector<Retired> retired;
};
