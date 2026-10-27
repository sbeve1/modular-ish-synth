#include "ui/Undo.h"

#include "ui/UiState.h"

static constexpr size_t MAX_STEPS = 200;
static constexpr double SETTLE_SECONDS = 0.35;
static uint64_t changeKey(UiState& ui);

void UndoHistory::capture(UiState& ui, Snapshot& out)
{
    for (int i = 0; i < NUM_PARAMS; ++i) out.norm[i] = ui.s.params.norm((P)i);
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) out.mods[i] = ui.s.mods.get(i);
    for (int o = 0; o < 2; ++o) out.wt[o] = ui.wt.get(o);
    for (int l = 0; l < 4; ++l) out.curves[l] = ui.wt.lfoCurve(l);
    out.fxOrder = ui.s.fxOrder.load(std::memory_order_relaxed);
    out.presetName = ui.presetName;
}

static bool sameSlot(const ModSlotData& a, const ModSlotData& b)
{
    if (!a.active() && !b.active()) return true;
    return a.src == b.src && a.aux == b.aux && a.target == b.target && a.amount == b.amount &&
           a.mode == b.mode && a.curve == b.curve;
}

bool UndoHistory::same(const Snapshot& a, const Snapshot& b)
{
    for (int i = 0; i < NUM_PARAMS; ++i) if (a.norm[i] != b.norm[i]) return false;
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) if (!sameSlot(a.mods[i], b.mods[i])) return false;
    for (int o = 0; o < 2; ++o) if (a.wt[o] != b.wt[o]) return false;
    for (int l = 0; l < 4; ++l) {
        if (a.curves[l].size() != b.curves[l].size()) return false;
        for (size_t k = 0; k < a.curves[l].size(); ++k) {
            const LfoPoint &p = a.curves[l][k], &q = b.curves[l][k];
            if (p.x != q.x || p.y != q.y || p.curve != q.curve) return false;
        }
    }
    return a.fxOrder == b.fxOrder;
}

void UndoHistory::apply(UiState& ui, const Snapshot& s)
{
    for (int i = 0; i < NUM_PARAMS; ++i) ui.s.params.setNorm((P)i, s.norm[i]);
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) ui.s.mods.set(i, s.mods[i]);
    for (int o = 0; o < 2; ++o) if (ui.wt.get(o) != s.wt[o] && s.wt[o]) ui.wt.set(o, s.wt[o]);
    for (int l = 0; l < 4; ++l) ui.wt.setLfoCurve(l, s.curves[l]);
    ui.s.fxOrder.store(s.fxOrder, std::memory_order_relaxed);
    ui.presetName = s.presetName;
    ui.presetDirty = true;
}

void UndoHistory::reset(UiState& ui)
{
    capture(ui, committed);
    lastKey = changeKey(ui);
    undoStack.clear();
    redoStack.clear();
    pending = false;
}

void UndoHistory::commitPending(UiState& ui)
{
    Snapshot cur;
    capture(ui, cur);
    if (same(cur, committed)) { pending = false; return; }
    undoStack.push_back(std::move(committed));
    if (undoStack.size() > MAX_STEPS) undoStack.erase(undoStack.begin());
    committed = std::move(cur);
    redoStack.clear();
    pending = false;
}

static uint64_t changeKey(UiState& ui)
{
    // params, mod slots, resources and FX order each have a cheap change marker
    uint64_t k = ui.s.params.generation();
    k = k * 1000003u + ui.s.mods.generation();
    k = k * 1000003u + ui.wt.generation();
    k = k * 1000003u + ui.s.fxOrder.load(std::memory_order_relaxed);
    return k;
}

void UndoHistory::update(UiState& ui, bool mouseDown, double now)
{
    // nothing is copied while nothing changes; a full snapshot is only taken at commit
    uint64_t key = changeKey(ui);
    if (key != lastKey) {
        lastKey = key;
        lastChange = now;   // keep pushing the deadline out while things are still moving
        pending = true;
    }
    if (pending && !mouseDown && now - lastChange > SETTLE_SECONDS)
        commitPending(ui);
}

bool UndoHistory::undo(UiState& ui)
{
    if (pending) commitPending(ui);
    if (undoStack.empty()) return false;
    redoStack.push_back(std::move(committed));
    committed = std::move(undoStack.back());
    undoStack.pop_back();
    apply(ui, committed);
    return true;
}

bool UndoHistory::redo(UiState& ui)
{
    if (pending) commitPending(ui);
    if (redoStack.empty()) return false;
    undoStack.push_back(std::move(committed));
    committed = std::move(redoStack.back());
    redoStack.pop_back();
    apply(ui, committed);
    return true;
}
