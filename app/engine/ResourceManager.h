#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "engine/Instrument.h"
#include "engine/LfoCurve.h"

// Owns the data the audio thread reads by pointer: wavetables and drawn LFO
// curves (UI thread only).
//
// set*() publishes new data with an atomic store. The old object is kept
// alive until the audio callback counter has advanced twice: the callback
// loads each pointer once per block, so after two more callbacks nothing
// can still be reading it.
class ResourceManager {
public:
    explicit ResourceManager(SharedState& s) : shared(s)
    {
        for (int l = 0; l < 4; ++l) setLfoCurve(l, defaultLfoCurve());
    }

    // ---- wavetables ----
    void set(int osc, std::shared_ptr<WavetableSet> wt)
    {
        retire(current[osc]);
        current[osc] = std::move(wt);
        shared.wavetables[osc].store(current[osc].get(), std::memory_order_release);
    }
    const std::shared_ptr<WavetableSet>& get(int osc) const { return current[osc]; }

    // ---- LFO curves ----
    void setLfoCurve(int lfo, std::vector<LfoPoint> pts)
    {
        sanitizeLfoCurve(pts);
        auto table = std::make_shared<LfoCurveTable>();
        renderLfoCurve(pts, *table);
        retire(curveTable[lfo]);
        curveTable[lfo] = table;
        curvePoints[lfo] = std::move(pts);
        shared.lfoCurves[lfo].store(table.get(), std::memory_order_release);
    }
    const std::vector<LfoPoint>& lfoCurve(int lfo) const { return curvePoints[lfo]; }

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
    struct Retired { std::shared_ptr<const void> obj; uint64_t epoch; };
    SharedState& shared;
    std::shared_ptr<WavetableSet> current[2];
    std::shared_ptr<LfoCurveTable> curveTable[4];
    std::vector<LfoPoint> curvePoints[4];
    std::vector<Retired> retired;

    template <typename T>
    void retire(const std::shared_ptr<T>& p)
    {
        if (p) retired.push_back({ p, shared.telemetry.callbacks.load(std::memory_order_acquire) });
    }
};
