#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "imgui_internal.h"
#include "host/MidiHost.h"
#include "ui/Theme.h"

namespace w {

static constexpr float KNOB_A0 = 0.75f * 3.14159265f;   // 7:30
static constexpr float KNOB_A1 = 2.25f * 3.14159265f;   // 4:30

static float angleFor(float n) { return KNOB_A0 + (KNOB_A1 - KNOB_A0) * std::clamp(n, 0.0f, 1.0f); }

void arc(ImDrawList* dl, ImVec2 c, float r, float a0, float a1, ImU32 col, float thickness)
{
    if (std::fabs(a1 - a0) < 1e-4f) return;
    if (a1 < a0) std::swap(a0, a1);
    int segs = std::max(4, (int)((a1 - a0) * r / 3.0f));
    dl->PathArcTo(c, r, a0, a1, segs);
    dl->PathStroke(col, ImDrawFlags_None, thickness);
}

int slotsForParam(UiState& ui, int param, int* out, int max)
{
    int n = 0;
    for (int i = 0; i < NUM_MOD_SLOTS && n < max; ++i) {
        ModSlotData d = ui.s.mods.get(i);
        if (d.active() && d.target == param) out[n++] = i;
    }
    return n;
}

int addModulation(UiState& ui, ModSrc src, int target, float amount)
{
    for (int i = 0; i < NUM_MOD_SLOTS; ++i) {
        ModSlotData d = ui.s.mods.get(i);
        if (d.active() && d.src == src && d.target == target) { ui.lastTouchedSlot = i; return i; }
    }
    int slot = ui.s.mods.firstFree();
    if (slot < 0) return -1;
    ModSlotData d;
    d.src = src;
    d.target = target;
    d.amount = amount;
    ui.s.mods.set(slot, d);
    ui.lastTouchedSlot = slot;
    ui.presetDirty = true;
    return slot;
}

// ---------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------
bool Knob(UiState& ui, P p, const char* label, float radius)
{
    const ParamInfo& info = paramInfo(p);
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushID((int)p + 7000);
    const float width = std::max(radius * 2.0f + 14.0f, 54.0f);
    const float textH = ImGui::GetFontSize() * 0.85f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##knob", ImVec2(width, radius * 2.0f + 10.0f + textH));
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    bool changed = false;

    int slots[8];
    int numSlots = slotsForParam(ui, (int)p, slots, 8);
    int editSlot = -1;
    if (numSlots > 0) {
        editSlot = slots[numSlots - 1];
        for (int i = 0; i < numSlots; ++i) if (slots[i] == ui.lastTouchedSlot) editSlot = ui.lastTouchedSlot;
    }

    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID idDrag = ImGui::GetID("drag"), idMod = ImGui::GetID("modedit");
    float n = ui.s.params.norm(p);

    if (ImGui::IsItemActivated()) {
        st->SetFloat(idDrag, n);
        st->SetInt(idMod, (io.KeyAlt && editSlot >= 0) ? 1 : 0);
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        float delta = -io.MouseDelta.y * (io.KeyShift ? 0.0008f : 0.0055f);
        if (st->GetInt(idMod) && editSlot >= 0) {
            ModSlotData d = ui.s.mods.get(editSlot);
            ui.s.mods.setAmount(editSlot, std::clamp(d.amount + delta * 1.5f, -1.0f, 1.0f));
            ui.lastTouchedSlot = editSlot;
        } else if (delta != 0.0f) {
            float dn = std::clamp(st->GetFloat(idDrag, n) + delta, 0.0f, 1.0f);
            st->SetFloat(idDrag, dn);
            ui.s.params.setNorm(p, dn);
            changed = true;
        }
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        ui.s.params.setValue(p, info.def);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        int steps = info.numSteps();
        float stepN = steps ? 1.0f / steps : (io.KeyShift ? 0.002f : 0.02f);
        ui.s.params.setNorm(p, n + io.MouseWheel * stepN);
        changed = true;
    }
    if (changed) ui.presetDirty = true;
    n = ui.s.params.norm(p);

    // ---- drawing ----
    const ImVec2 c(pos.x + width * 0.5f, pos.y + radius + 4.0f);
    const bool bipolar = info.min < 0.0f && info.max > 0.0f && info.map == Map::Lin;
    const float aVal = angleFor(n);
    const float aBase = bipolar ? angleFor(0.5f) : KNOB_A0;

    // drop highlight
    if (ImGui::GetDragDropPayload() && ImGui::GetDragDropPayload()->IsDataType(MODSRC_PAYLOAD))
        dl->AddCircle(c, radius + 7.0f, hovered ? theme::mod : theme::modFaint, 32, hovered ? 2.0f : 1.0f);

    dl->AddCircleFilled(c, radius * 0.74f, (hovered || active) ? theme::knobBodyHi : theme::knobBody, 32);
    dl->AddCircle(c, radius * 0.74f, theme::withAlpha(theme::border, 1.0f), 32, 1.0f);

    arc(dl, c, radius, KNOB_A0, KNOB_A1, theme::track, 3.5f);
    arc(dl, c, radius, aBase, aVal, theme::accent, 3.5f);

    // modulation rings: one per slot (max 3 drawn)
    for (int k = 0; k < std::min(numSlots, 3); ++k) {
        ModSlotData d = ui.s.mods.get(slots[k]);
        float amt = d.mode == ModMode::Multiply ? d.amount * n : d.amount;
        float r = radius + 4.5f + k * 3.5f;
        ImU32 col = slots[k] == editSlot ? theme::mod : theme::modDim;
        float lo = n, hi = n + amt;
        if (modSrcBipolar(d.src)) lo = n - amt;
        arc(dl, c, r, angleFor(lo), angleFor(hi), col, 2.2f);
    }

    // live value from the newest voice
    if (numSlots > 0) {
        float live = ui.s.telemetry.modNorm[(int)p].load(std::memory_order_relaxed);
        bool show = info.scope == Scope::Global || ui.s.telemetry.displayVoiceActive.load(std::memory_order_relaxed);
        if (show) {
            float a = angleFor(live);
            ImVec2 d(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius);
            dl->AddCircleFilled(d, 3.6f, theme::mod);
            dl->AddCircleFilled(d, 1.6f, IM_COL32_WHITE);
        }
    }

    // pointer
    ImVec2 p0(c.x + std::cos(aVal) * radius * 0.22f, c.y + std::sin(aVal) * radius * 0.22f);
    ImVec2 p1(c.x + std::cos(aVal) * radius * 0.66f, c.y + std::sin(aVal) * radius * 0.66f);
    dl->AddLine(p0, p1, theme::text, 2.2f);

    // MIDI learn pulse
    if (ui.midi.learning() == (int)p) {
        float pulse = 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 8.0f);
        dl->AddCircle(c, radius + 8.0f, theme::withAlpha(theme::warn, 0.4f + 0.6f * pulse), 32, 2.0f);
    }

    // label / value
    ImGui::PushFont(theme::fontSmall);
    std::string text = (hovered || active) ? info.format(info.toValue(n)) : std::string(label ? label : info.label);
    ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, pos.y + radius * 2.0f + 8.0f),
                (hovered || active) ? theme::accent : theme::textDim, text.c_str());
    ImGui::PopFont();

    // drop target: create a route
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload(MODSRC_PAYLOAD)) {
            ModSrc src = *(const ModSrc*)pl->Data;
            addModulation(ui, src, (int)p, modSrcBipolar(src) ? 0.2f : 0.3f);
        }
        ImGui::EndDragDropTarget();
    }

    // tooltip
    if (hovered && !active && ImGui::GetCurrentContext()->HoveredIdTimer > 0.6f && !ImGui::GetDragDropPayload()) {
        ImGui::BeginTooltip();
        ImGui::PushFont(theme::fontBold);
        ImGui::TextUnformatted(info.label);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::accent), "%s", info.format(info.toValue(n)).c_str());
        for (int k = 0; k < numSlots; ++k) {
            ModSlotData d = ui.s.mods.get(slots[k]);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::mod), "%s  %+.0f%%", modSrcName(d.src), d.amount * 100.0f);
        }
        int cc = ui.midi.ccForParam((int)p);
        if (cc >= 0) ImGui::TextDisabled("MIDI CC %d", cc);
        ImGui::PushFont(theme::fontSmall);
        ImGui::TextDisabled("Drag  |  Shift fine  |  Double-click reset%s  |  Right-click menu",
                            numSlots ? "  |  Alt+drag depth" : "");
        ImGui::PopFont();
        ImGui::EndTooltip();
    }

    // context menu
    if (ImGui::BeginPopupContextItem("##ctx")) {
        ImGui::PushFont(theme::fontBold);
        ImGui::TextUnformatted(info.label);
        ImGui::PopFont();
        ImGui::Separator();
        if (ImGui::MenuItem("Reset to default")) ui.s.params.setValue(p, info.def);
        int cc = ui.midi.ccForParam((int)p);
        if (ImGui::MenuItem(ui.midi.learning() == (int)p ? "Waiting for a CC... (click to cancel)" : "MIDI Learn")) {
            ui.midi.armLearn(ui.midi.learning() == (int)p ? -1 : (int)p);
        }
        if (cc >= 0) {
            char buf[48];
            std::snprintf(buf, sizeof buf, "Clear MIDI CC %d", cc);
            if (ImGui::MenuItem(buf)) ui.midi.clearMapping((int)p);
        }
        if (numSlots > 0) {
            ImGui::Separator();
            ImGui::TextDisabled("Modulation");
            for (int k = 0; k < numSlots; ++k) {
                ImGui::PushID(slots[k]);
                ModSlotData d = ui.s.mods.get(slots[k]);
                float pct = d.amount * 100.0f;
                ImGui::SetNextItemWidth(150);
                if (ImGui::SliderFloat(modSrcName(d.src), &pct, -100.0f, 100.0f, "%+.0f%%")) {
                    ui.s.mods.setAmount(slots[k], pct / 100.0f);
                    ui.lastTouchedSlot = slots[k];
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) ui.s.mods.clear(slots[k]);
                ImGui::PopID();
            }
            if (ImGui::MenuItem("Remove all modulation"))
                for (int k = 0; k < numSlots; ++k) ui.s.mods.clear(slots[k]);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return changed;
}

// ---------------------------------------------------------------------
// Toggles, choices, steppers
// ---------------------------------------------------------------------
bool Toggle(UiState& ui, P p, const char* label)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID((int)p + 9000);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = 15.0f, wSw = 27.0f;
    ImGui::PushFont(theme::fontSmall);
    ImVec2 ts = ImGui::CalcTextSize(label);
    ImGui::InvisibleButton("##t", ImVec2(wSw + 6 + ts.x, std::max(h, ts.y)));
    bool on = ui.s.params.norm(p) > 0.5f;
    bool clicked = ImGui::IsItemClicked();
    if (clicked) { on = !on; ui.s.params.setNorm(p, on ? 1.0f : 0.0f); ui.presetDirty = true; }

    float t = on ? 1.0f : 0.0f;
    ImU32 bgc = on ? theme::accentDim : theme::track;
    dl->AddRectFilled(pos, ImVec2(pos.x + wSw, pos.y + h), bgc, h * 0.5f);
    ImVec2 knob(pos.x + h * 0.5f + t * (wSw - h), pos.y + h * 0.5f);
    dl->AddCircleFilled(knob, h * 0.5f - 2.0f, on ? theme::accent : theme::textDim);
    dl->AddText(ImVec2(pos.x + wSw + 6, pos.y + (h - ts.y) * 0.5f), ImGui::IsItemHovered() ? theme::text : theme::textDim, label);
    ImGui::PopFont();
    ImGui::PopID();
    return clicked;
}

bool Power(UiState& ui, P p, float size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID((int)p + 11000);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##pw", ImVec2(size, size));
    bool on = ui.s.params.norm(p) > 0.5f;
    if (ImGui::IsItemClicked()) { on = !on; ui.s.params.setNorm(p, on ? 1.0f : 0.0f); ui.presetDirty = true; }
    ImVec2 c(pos.x + size * 0.5f, pos.y + size * 0.5f);
    float r = size * 0.5f;
    ImU32 col = on ? theme::accent : (ImGui::IsItemHovered() ? theme::textDim : theme::textFaint);
    if (on) dl->AddCircleFilled(c, r + 2.0f, theme::accentFaint);
    dl->PathArcTo(c, r * 0.62f, -3.14159f * 0.5f + 0.7f, 3.14159f * 1.5f - 0.7f, 16);
    dl->PathStroke(col, 0, 1.8f);
    dl->AddLine(ImVec2(c.x, c.y - r * 0.8f), ImVec2(c.x, c.y - r * 0.1f), col, 1.8f);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %s", paramInfo(p).label, on ? "On" : "Off");
    ImGui::PopID();
    return on;
}

bool Choice(UiState& ui, P p, float width)
{
    const ParamInfo& info = paramInfo(p);
    ImGui::PushID((int)p + 13000);
    int cur = (int)std::lround(ui.s.params.value(p));
    std::string preview = info.format((float)cur);
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(theme::rgba(30, 36, 48)));
    if (ImGui::BeginCombo("##c", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (int v = (int)info.min; v <= (int)info.max; ++v) {
            bool sel = v == cur;
            if (ImGui::Selectable(info.format((float)v).c_str(), sel)) {
                ui.s.params.setValue(p, (float)v);
                changed = true;
                ui.presetDirty = true;
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleColor();
    ImGui::PopID();
    return changed;
}

bool Stepper(UiState& ui, P p, float width, const char* label)
{
    const ParamInfo& info = paramInfo(p);
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID((int)p + 15000);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float h = 22.0f;
    ImGui::InvisibleButton("##s", ImVec2(width, h));
    bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    bool changed = false;
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID id = ImGui::GetID("acc");
    float n = ui.s.params.norm(p);
    int steps = std::max(info.numSteps(), 1);

    if (ImGui::IsItemActivated()) st->SetFloat(id, n);
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        float acc = std::clamp(st->GetFloat(id, n) + (io.MouseDelta.x - io.MouseDelta.y) * (0.6f / (steps * 6.0f)), 0.0f, 1.0f);
        st->SetFloat(id, acc);
        ui.s.params.setNorm(p, acc);
        changed = true;
    }
    if (hovered && io.MouseWheel != 0.0f) { ui.s.params.setNorm(p, n + io.MouseWheel / steps); changed = true; }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { ui.s.params.setValue(p, info.def); changed = true; }
    if (changed) ui.presetDirty = true;

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + h), hovered || active ? theme::rgba(40, 47, 63) : theme::rgba(30, 36, 48), 4.0f);
    std::string v = info.format(ui.s.params.value(p));
    if (info.unit == Unit::Oct || info.unit == Unit::Semi) {   // compact: "+1" not "+1 oct"
        char b[16]; std::snprintf(b, sizeof b, "%+.0f", ui.s.params.value(p)); v = b;
    }
    ImGui::PushFont(theme::fontSmall);
    const char* lab = label ? label : info.label;
    ImVec2 ls = ImGui::CalcTextSize(lab), vs = ImGui::CalcTextSize(v.c_str());
    dl->AddText(ImVec2(pos.x + 6, pos.y + (h - ls.y) * 0.5f), theme::textDim, lab);
    dl->AddText(ImVec2(pos.x + width - vs.x - 6, pos.y + (h - vs.y) * 0.5f), theme::accent, v.c_str());
    ImGui::PopFont();
    if (hovered && !active && ImGui::GetCurrentContext()->HoveredIdTimer > 0.6f)
        ImGui::SetTooltip("%s: drag or scroll to change, double-click to reset", info.label);
    ImGui::PopID();
    return changed;
}

// ---------------------------------------------------------------------
// Mod source handle
// ---------------------------------------------------------------------
void ModSourceHandle(UiState& ui, ModSrc src, float width, const char* label)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID((int)src + 17000);
    ImGui::PushFont(theme::fontSmall);
    const char* text = label ? label : modSrcShortName(src);
    ImVec2 ts = ImGui::CalcTextSize(text);
    float wd = width > 0 ? width : ts.x + 30.0f, h = 20.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##src", ImVec2(wd, h));
    bool hovered = ImGui::IsItemHovered();

    float v = ui.s.telemetry.srcValue[(int)src].load(std::memory_order_relaxed);
    bool bip = modSrcBipolar(src);
    dl->AddRectFilled(pos, ImVec2(pos.x + wd, pos.y + h), hovered ? theme::rgba(52, 40, 80) : theme::rgba(38, 31, 58), h * 0.5f);
    // live activity fill
    float fill = bip ? std::fabs(v) : std::clamp(v, 0.0f, 1.0f);
    if (fill > 0.001f)
        dl->AddRectFilled(pos, ImVec2(pos.x + wd * fill, pos.y + h), theme::modFaint, h * 0.5f);
    dl->AddRect(pos, ImVec2(pos.x + wd, pos.y + h), hovered ? theme::mod : theme::modDim, h * 0.5f, 0, 1.0f);
    // grip dots
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 2; ++c)
            dl->AddCircleFilled(ImVec2(pos.x + 9 + c * 4, pos.y + 6 + r * 4), 1.2f, theme::mod);
    dl->AddText(ImVec2(pos.x + 20, pos.y + (h - ts.y) * 0.5f), theme::text, text);
    ImGui::PopFont();

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload(MODSRC_PAYLOAD, &src, sizeof src);
        ImGui::Text("Modulate with %s", modSrcName(src));
        ImGui::TextDisabled("Drop on any knob");
        ImGui::EndDragDropSource();
    }
    if (hovered && !ImGui::GetDragDropPayload())
        ImGui::SetTooltip("%s\nDrag onto a knob to modulate it", modSrcName(src));
    ImGui::PopID();
}

// ---------------------------------------------------------------------
// Panels and meters
// ---------------------------------------------------------------------
bool PanelBegin(UiState& ui, const char* id, const char* title, ImVec2 size, int powerParam)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 6));
    bool open = ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (!open) return false;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + ws.x, wp.y + 28), theme::panelHi, theme::panelHi, theme::panel, theme::panel);
    dl->AddLine(ImVec2(wp.x + 1, wp.y + 28), ImVec2(wp.x + ws.x - 1, wp.y + 28), theme::border);

    bool on = true;
    if (powerParam >= 0) {
        ImGui::SetCursorPos(ImVec2(10, 6.5f));
        on = Power(ui, (P)powerParam);
        ImGui::SameLine(0, 8);
    } else {
        ImGui::SetCursorPos(ImVec2(10, 5));
    }
    ImGui::SetCursorPosY(5);
    ImGui::PushFont(theme::fontBold);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(on ? theme::text : theme::textFaint), "%s", title);
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(10, 36));
    return true;
}

void PanelEnd() { ImGui::EndChild(); }

void SectionLabel(const char* text)
{
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::textDim), "%s", text);
    ImGui::PopFont();
}

void LevelMeter(float level, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), theme::rgba(16, 19, 26), 2.0f);
    float db = 20.0f * std::log10(std::max(level, 1e-5f));
    float t = std::clamp((db + 48.0f) / 48.0f, 0.0f, 1.0f);
    float top = pos.y + size.y * (1.0f - t);
    ImU32 col = db > -1.0f ? theme::hot : db > -9.0f ? theme::warn : theme::accent;
    dl->AddRectFilledMultiColor(ImVec2(pos.x, top), ImVec2(pos.x + size.x, pos.y + size.y),
                                theme::withAlpha(col, 1.0f), theme::withAlpha(col, 1.0f),
                                theme::withAlpha(theme::accent, 0.5f), theme::withAlpha(theme::accent, 0.5f));
    for (int mark : { -6, -12, -24, -36 }) {
        float y = pos.y + size.y * (1.0f - (mark + 48.0f) / 48.0f);
        dl->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), theme::rgba(0, 0, 0, 120));
    }
}

} // namespace w
