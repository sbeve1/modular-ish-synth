#pragma once

#include <memory>
#include <string>
#include <vector>

#include "engine/LfoCurve.h"
#include "engine/Params.h"
#include "engine/Shared.h"
#include "engine/Wavetable.h"

struct UiState;

// Snapshot-based undo/redo for the whole sound (UI thread only).
//
// Each frame the current state is compared with the last committed one.
// Changes are committed as one undo step once they settle: the mouse is up
// and nothing changed for a moment. So a knob drag, a MIDI CC sweep or a
// preset load each become a single step, whatever made the change.
class UndoHistory {
public:
    void reset(UiState& ui);                        // start from the current state, clear history
    void update(UiState& ui, bool mouseDown, double now);
    bool undo(UiState& ui);
    bool redo(UiState& ui);
    bool canUndo() const { return !undoStack.empty() || pending; }
    bool canRedo() const { return !redoStack.empty(); }

private:
    struct Snapshot {
        float norm[NUM_PARAMS];
        ModSlotData mods[NUM_MOD_SLOTS];
        std::shared_ptr<WavetableSet> wt[2];
        std::vector<LfoPoint> curves[4];
        uint32_t fxOrder = 0;
        std::string presetName;
    };

    Snapshot committed;
    uint64_t lastKey = 0;       // change counters seen last frame; capture only when they move
    bool pending = false;
    double lastChange = 0.0;
    std::vector<Snapshot> undoStack, redoStack;

    static void capture(UiState& ui, Snapshot& out);
    static void apply(UiState& ui, const Snapshot& s);
    static bool same(const Snapshot& a, const Snapshot& b);
    void commitPending(UiState& ui);
};
