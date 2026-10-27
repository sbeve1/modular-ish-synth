#include "ui/App.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "host/AudioHost.h"
#include "host/MidiHost.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/Theme.h"
#include "ui/FxPage.h"
#include "ui/Views.h"
#include "ui/WavetableEditor.h"
#include "ui/Widgets.h"

namespace fs = std::filesystem;

namespace app {

// ---------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------
int presetCount(const UiState& ui) { return (int)(ui.factory.size() + ui.userFiles.size()); }

static std::string presetLabel(const UiState& ui, int i)
{
    if (i < (int)ui.factory.size()) return ui.factory[i].name;
    return fs::path(ui.userFiles[i - ui.factory.size()]).stem().string();
}

void loadPreset(UiState& ui, int index)
{
    int n = presetCount(ui);
    if (n == 0) return;
    index = ((index % n) + n) % n;
    ui.s.uiEvents.push({ EventType::AllNotesOff });

    if (index < (int)ui.factory.size()) {
        applyPreset(ui.factory[index], ui.s, ui.wt);
        ui.presetName = ui.factory[index].name;
    } else {
        Preset p;
        if (!loadPresetFile(p, ui.userFiles[index - ui.factory.size()])) {
            ui.setStatus("Could not read preset file", ImGui::GetTime());
            return;
        }
        applyPreset(p, ui.s, ui.wt);
        ui.presetName = p.name;
    }
    ui.presetIndex = index;
    ui.presetDirty = false;
    ui.lastTouchedSlot = -1;
}

void init(UiState& ui)
{
    ui.factory = factoryPresets();
    ui.userFiles = listUserPresets();
    loadPreset(ui, 0);
    ui.undo.reset(ui);
}

static void savePresetAs(UiState& ui, const char* name)
{
    Preset p;
    capturePreset(p, ui.s, ui.wt);
    p.name = name;
    p.category = "User";
    std::string safe;
    for (const char* c = name; *c; ++c) safe += (std::isalnum((unsigned char)*c) || *c == ' ' || *c == '-' || *c == '_') ? *c : '_';
    if (safe.empty()) safe = "Untitled";
    std::string path = (fs::path(userPresetDir()) / (safe + ".msp")).string();
    if (savePresetFile(p, path)) {
        ui.userFiles = listUserPresets();
        for (size_t i = 0; i < ui.userFiles.size(); ++i)
            if (ui.userFiles[i] == path) ui.presetIndex = (int)(ui.factory.size() + i);
        ui.presetName = name;
        ui.presetDirty = false;
        ui.setStatus("Saved " + path, ImGui::GetTime(), 4.0);
    } else {
        ui.setStatus("Could not save preset", ImGui::GetTime());
    }
}

// ---------------------------------------------------------------------
// Computer keyboard
// ---------------------------------------------------------------------
// Called straight from the GLFW key callback, the moment Windows delivers the
// key (not once per drawn frame), so a key press reaches the audio thread
// within about a millisecond.
static int keyIndex(int glfwKey)
{
    static const int keys[] = { 'A', 'W', 'S', 'E', 'D', 'F', 'T', 'G', 'Y', 'H', 'U', 'J', 'K', 'O', 'L', 'P' };
    for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i)
        if (keys[i] == glfwKey) return i;
    return -1;
}
constexpr int NUM_NOTE_KEYS = 16;
static int gKeyNote[NUM_NOTE_KEYS] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

void releaseKeys(UiState& ui)
{
    for (int i = 0; i < NUM_NOTE_KEYS; ++i)
        if (gKeyNote[i] >= 0) { ui.s.uiEvents.push({ EventType::NoteOff, (uint8_t)gKeyNote[i], 0 }); gKeyNote[i] = -1; }
}

void onKey(UiState& ui, int key, int action, int ctrlOrAlt)
{
    constexpr int PRESS = 1, RELEASE = 0, REPEAT = 2;   // GLFW_PRESS / GLFW_RELEASE / GLFW_REPEAT
    int i = keyIndex(key);

    // releases always go through, so a note never sticks
    if (action == RELEASE) {
        if (i >= 0 && gKeyNote[i] >= 0) {
            ui.s.uiEvents.push({ EventType::NoteOff, (uint8_t)gKeyNote[i], 0 });
            gKeyNote[i] = -1;
        }
        return;
    }
    if (ImGui::GetIO().WantTextInput || ctrlOrAlt) return;

    if (i >= 0) {
        if (action == PRESS && gKeyNote[i] < 0) {
            int note = std::clamp((ui.keyOctave + 1) * 12 + i, 0, 127);
            ui.s.uiEvents.push({ EventType::NoteOn, (uint8_t)note, (uint8_t)ui.keyVelocity });
            gKeyNote[i] = note;
        }
        return;
    }
    double now = ImGui::GetTime();
    if (action == PRESS && key == 'Z') { ui.keyOctave = std::max(ui.keyOctave - 1, 0); ui.setStatus("Keyboard octave C" + std::to_string(ui.keyOctave), now); }
    if (action == PRESS && key == 'X') { ui.keyOctave = std::min(ui.keyOctave + 1, 8); ui.setStatus("Keyboard octave C" + std::to_string(ui.keyOctave), now); }
    if ((action == PRESS || action == REPEAT) && key == 'C') { ui.keyVelocity = std::max(ui.keyVelocity - 10, 10); ui.setStatus("Keyboard velocity " + std::to_string(ui.keyVelocity), now); }
    if ((action == PRESS || action == REPEAT) && key == 'V') { ui.keyVelocity = std::min(ui.keyVelocity + 10, 127); ui.setStatus("Keyboard velocity " + std::to_string(ui.keyVelocity), now); }
}

// ---------------------------------------------------------------------
// Small layout helpers
// ---------------------------------------------------------------------
static void knobRow(UiState& ui, std::initializer_list<P> params, float radius = 18.0f)
{
    bool first = true;
    for (P p : params) {
        if (!first) ImGui::SameLine(0, 4);
        w::Knob(ui, p, nullptr, radius);
        first = false;
    }
}

static bool tabButton(const char* label, bool selected, float width, float height = 22.0f)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, ImVec2(width, height));
    bool hov = ImGui::IsItemHovered();
    if (selected) dl->AddRectFilled(a, ImVec2(a.x + width, a.y + height), theme::rgba(36, 36, 37), 4.0f);
    else if (hov) dl->AddRectFilled(a, ImVec2(a.x + width, a.y + height), theme::rgba(29, 29, 30), 4.0f);
    ImGui::PushFont(theme::fontBold);
    ImVec2 ts = ImGui::CalcTextSize(label, nullptr, true);
    dl->AddText(ImVec2(a.x + (width - ts.x) * 0.5f, a.y + (height - ts.y) * 0.5f),
                selected ? theme::accent : (hov ? theme::text : theme::textDim), label, ImGui::FindRenderedTextEnd(label));
    ImGui::PopFont();
    if (selected) dl->AddLine(ImVec2(a.x + 6, a.y + height - 1), ImVec2(a.x + width - 6, a.y + height - 1), theme::accent, 2.0f);
    return clicked;
}

// ---------------------------------------------------------------------
// Header
// ---------------------------------------------------------------------
static void drawHeader(UiState& ui, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    const float H = 58.0f;
    dl->AddRectFilled(o, ImVec2(o.x + width, o.y + H), theme::panel);
    dl->AddLine(ImVec2(o.x, o.y + H), ImVec2(o.x + width, o.y + H), theme::border);

    // logo
    ImGui::SetCursorScreenPos(ImVec2(o.x + 16, o.y + 9));
    ImGui::PushFont(theme::fontTitle);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::accent), "MODULAR_SYNTH");
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(o.x + 17, o.y + 35));
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::textFaint), "WAVETABLE SYNTHESIZER");
    ImGui::PopFont();

    // preset browser
    float px = o.x + 190;
    ImGui::SetCursorScreenPos(ImVec2(px, o.y + 16));
    if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) loadPreset(ui, ui.presetIndex - 1);
    ImGui::SameLine(0, 4);
    std::string label = ui.presetName + (ui.presetDirty ? " *" : "");
    ImGui::SetNextItemWidth(250);
    if (ImGui::BeginCombo("##preset", label.c_str(), ImGuiComboFlags_HeightLargest)) {
        std::string lastCat;
        for (int i = 0; i < presetCount(ui); ++i) {
            std::string cat = i < (int)ui.factory.size() ? ui.factory[i].category : "User";
            if (cat != lastCat) { ImGui::SeparatorText(cat.c_str()); lastCat = cat; }
            if (ImGui::Selectable(presetLabel(ui, i).c_str(), i == ui.presetIndex)) loadPreset(ui, i);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 4);
    if (ImGui::ArrowButton("##next", ImGuiDir_Right)) loadPreset(ui, ui.presetIndex + 1);
    ImGui::SameLine(0, 8);
    if (ImGui::Button("Save")) ImGui::OpenPopup("Save Preset");
    ImGui::SameLine(0, 4);
    if (ImGui::Button("Init")) loadPreset(ui, 0);
    ImGui::SameLine(0, 12);
    ImGui::BeginDisabled(!ui.undo.canUndo());
    if (ImGui::Button("Undo")) ui.undo.undo(ui);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Undo (Ctrl+Z)");
    ImGui::SameLine(0, 4);
    ImGui::BeginDisabled(!ui.undo.canRedo());
    if (ImGui::Button("Redo")) ui.undo.redo(ui);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Redo (Ctrl+Y)");

    if (ImGui::BeginPopupModal("Save Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char name[64] = "";
        if (ImGui::IsWindowAppearing()) {
            std::snprintf(name, sizeof name, "%s", ui.presetName.c_str());
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::TextDisabled("Saved to %s", userPresetDir().c_str());
        ImGui::SetNextItemWidth(320);
        bool enter = ImGui::InputText("##name", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue);
        if (ImGui::Button("Save", ImVec2(100, 0)) || enter) { savePresetAs(ui, name); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // page tabs
    const char* pages[] = { "SYNTH", "MATRIX", "FX", "GLOBAL" };
    float tx = px + 560;
    for (int i = 0; i < 4; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(tx + i * 82, o.y + 17));
        if (tabButton(pages[i], ui.page == i, 76, 24)) ui.page = i;
    }

    // right side: scope, status, master, meters
    float rx = o.x + width - 16;
    float mL = ui.s.telemetry.peakL.exchange(0.0f), mR = ui.s.telemetry.peakR.exchange(0.0f);
    ui.peakL = std::max(mL, ui.peakL * 0.9f);
    ui.peakR = std::max(mR, ui.peakR * 0.9f);
    ImGui::SetCursorScreenPos(ImVec2(rx - 7, o.y + 8));
    w::LevelMeter(ui.peakR, ImVec2(6, 42));
    ImGui::SetCursorScreenPos(ImVec2(rx - 15, o.y + 8));
    w::LevelMeter(ui.peakL, ImVec2(6, 42));

    ImGui::SetCursorScreenPos(ImVec2(rx - 80, o.y + 1));
    w::Knob(ui, P::MASTER_VOL, "Master", 15.0f);

    ImGui::SetCursorScreenPos(ImVec2(rx - 240, o.y + 8));
    views::Oscilloscope(ui, ImVec2(150, 42));

    ImGui::PushFont(theme::fontSmall);
    float cpu = ui.s.telemetry.cpuLoad.load() * 100.0f;
    int voices = ui.s.telemetry.activeVoices.load();
    char buf[64];
    std::snprintf(buf, sizeof buf, "CPU %4.1f%%", cpu);
    dl->AddText(ImVec2(rx - 330, o.y + 10), cpu > 70 ? theme::warn : theme::textDim, buf);
    std::snprintf(buf, sizeof buf, "VOICES %d", voices);
    dl->AddText(ImVec2(rx - 330, o.y + 25), theme::textDim, buf);
    static uint32_t lastAct = 0;
    static double actTime = -1.0;
    uint32_t act = ui.midi.activity();
    if (act != lastAct) { lastAct = act; actTime = ImGui::GetTime(); }
    float led = (float)std::max(0.0, 1.0 - (ImGui::GetTime() - actTime) * 6.0);
    ImVec2 lc(rx - 326, o.y + 46);
    dl->AddCircleFilled(lc, 3.5f, theme::lerpColor(theme::rgba(40, 40, 41), theme::accent, led));
    dl->AddText(ImVec2(rx - 318, o.y + 39), theme::textFaint, ui.midi.openPort() >= 0 ? "MIDI" : "NO MIDI");
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + H + 8));
}

// ---------------------------------------------------------------------
// Synth page
// ---------------------------------------------------------------------
static void oscPanel(UiState& ui, int osc, ImVec2 size)
{
    char id[16];
    std::snprintf(id, sizeof id, "osc%d", osc);
    if (!w::PanelBegin(ui, id, osc == 0 ? "OSC A" : "OSC B", size, (int)oscParam(osc, P::OSCA_ON))) { w::PanelEnd(); return; }
    float innerW = size.x - 20;

    // wavetable selector
    ImGui::PushID(osc);
    const auto& cur = ui.wt.get(osc);
    ImGui::SetNextItemWidth(innerW - 66);
    if (ImGui::BeginCombo("##wt", cur ? cur->name.c_str() : "-")) {
        for (int i = 0; i < numFactoryWavetables(); ++i)
            if (ImGui::Selectable(factoryWavetableName(i), cur && cur->name == factoryWavetableName(i))) {
                ui.wt.set(osc, makeFactoryWavetable(i));
                ui.presetDirty = true;
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 6);
    if (ImGui::Button("EDIT", ImVec2(60, 0))) { ui.wtEditorOpen = true; ui.wtEditorOsc = osc; }
    ImGui::PopID();

    views::WavetableView(ui, osc, ImVec2(innerW, std::max(128.0f, size.y - 36 - 30 - 196)));
    ImGui::Dummy(ImVec2(0, 2));

    knobRow(ui, { oscParam(osc, P::OSCA_POS), oscParam(osc, P::OSCA_LEVEL), oscParam(osc, P::OSCA_PAN),
                  oscParam(osc, P::OSCA_DETUNE), oscParam(osc, P::OSCA_BLEND), oscParam(osc, P::OSCA_WIDTH) });

    float sw = (innerW - 12) / 3.0f;
    w::Stepper(ui, oscParam(osc, P::OSCA_OCT), sw, "Oct"); ImGui::SameLine(0, 6);
    w::Stepper(ui, oscParam(osc, P::OSCA_SEMI), sw, "Semi"); ImGui::SameLine(0, 6);
    w::Stepper(ui, oscParam(osc, P::OSCA_UNISON), sw, "Unison");

    knobRow(ui, { oscParam(osc, P::OSCA_FINE), oscParam(osc, P::OSCA_PHASE), oscParam(osc, P::OSCA_RAND),
                  oscParam(osc, P::OSCA_WARPAMT) });
    ImGui::SameLine(0, 8);
    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0, 2));
    w::SectionLabel("Warp");
    w::Choice(ui, oscParam(osc, P::OSCA_WARP), 90);
    w::Toggle(ui, oscParam(osc, P::OSCA_FILTER), "Filter");
    ImGui::EndGroup();

    w::PanelEnd();
}

static void subNoisePanels(UiState& ui, ImVec2 size)
{
    float h1 = (size.y - 8) * 0.5f;
    if (w::PanelBegin(ui, "sub", "SUB", ImVec2(size.x, h1), (int)P::SUB_ON)) {
        float cw = size.x - 20;
        w::Choice(ui, P::SUB_SHAPE, cw * 0.55f); ImGui::SameLine(0, 6);
        w::Stepper(ui, P::SUB_OCT, cw * 0.45f - 6, "Oct");
        knobRow(ui, { P::SUB_LEVEL, P::SUB_PAN }, 17.0f);
        w::Toggle(ui, P::SUB_FILTER, "Filter");
    }
    w::PanelEnd();
    if (w::PanelBegin(ui, "noise", "NOISE", ImVec2(size.x, h1), (int)P::NOISE_ON)) {
        w::Choice(ui, P::NOISE_TYPE, size.x - 20);
        knobRow(ui, { P::NOISE_LEVEL, P::NOISE_PAN }, 17.0f);
        w::Toggle(ui, P::NOISE_FILTER, "Filter");
    }
    w::PanelEnd();
}

static void filterPanel(UiState& ui, ImVec2 size)
{
    if (w::PanelBegin(ui, "filter", "FILTERS", size)) {
        float cw = size.x - 20;
        // header: one tab per filter (with its power switch) + routing
        ImGui::PushFont(theme::fontBold);
        float titleW = ImGui::CalcTextSize("FILTERS").x;
        ImGui::PopFont();
        ImGui::SetCursorPos(ImVec2(10 + titleW + 18, 4));
        for (int f = 0; f < 2; ++f) {
            if (f) ImGui::SameLine(0, 10);
            ImGui::SetCursorPosY(6.5f);
            w::Power(ui, fltParam(f, P::FLT_ON), 14.0f);
            ImGui::SameLine(0, 4);
            ImGui::SetCursorPosY(4);
            if (tabButton(f == 0 ? "FILTER 1" : "FILTER 2", ui.filterTab == f, 70)) ui.filterTab = f;
        }
        ImGui::SameLine(0, 14);
        ImGui::SetCursorPosY(3);
        w::Choice(ui, P::FLT_ROUTING, 90);
        ImGui::SetCursorPos(ImVec2(10, 36));

        int f = ui.filterTab;
        w::Choice(ui, fltParam(f, P::FLT_TYPE), 110);
        ImGui::SameLine(0, 10);
        ImGui::PushFont(theme::fontSmall);
        ImGui::TextDisabled("drag the graph: cutoff / resonance");
        ImGui::PopFont();
        views::FilterResponse(ui, f, ImVec2(cw, std::max(140.0f, size.y - 36 - 30 - 112)));
        ImGui::Dummy(ImVec2(0, 4));
        knobRow(ui, { fltParam(f, P::FLT_CUTOFF), fltParam(f, P::FLT_RES), fltParam(f, P::FLT_DRIVE),
                      fltParam(f, P::FLT_KEYTRK), fltParam(f, P::FLT_MIX) }, 20.0f);
    }
    w::PanelEnd();
}

static void envPanel(UiState& ui, ImVec2 size)
{
    if (w::PanelBegin(ui, "env", "ENVELOPES", size)) {
        float cw = size.x - 20;
        ImGui::SetCursorPos(ImVec2(110, 4));
        const char* names[] = { "ENV 1 AMP", "ENV 2", "ENV 3" };
        for (int i = 0; i < 3; ++i) {
            ImGui::SameLine(0, 4);
            if (tabButton(names[i], ui.envTab == i, i == 0 ? 86 : 60)) ui.envTab = i;
        }
        ImGui::SameLine(0, 10);
        ImGui::SetCursorPosY(4);
        w::ModSourceHandle(ui, (ModSrc)((int)ModSrc::Env1 + ui.envTab), 70);
        ImGui::SetCursorPos(ImVec2(10, 36));

        int e = ui.envTab;
        views::EnvelopeEditor(ui, e, ImVec2(cw, size.y - 36 - 78));
        ImGui::Dummy(ImVec2(0, 2));
        knobRow(ui, { envParam(e, P::ENV1_ATK), envParam(e, P::ENV1_HOLD), envParam(e, P::ENV1_DEC),
                      envParam(e, P::ENV1_SUS), envParam(e, P::ENV1_REL) }, 17.0f);
        if (e == 0) {
            ImGui::SameLine(0, 10);
            w::Knob(ui, P::VEL_SENS, "Velocity", 17.0f);
        }
    }
    w::PanelEnd();
}

static void lfoPanel(UiState& ui, ImVec2 size)
{
    if (w::PanelBegin(ui, "lfo", "LFO", size)) {
        float cw = size.x - 20;
        ImGui::SetCursorPos(ImVec2(60, 4));
        for (int i = 0; i < 4; ++i) {
            char n[8]; std::snprintf(n, sizeof n, "LFO %d", i + 1);
            ImGui::SameLine(0, 4);
            if (tabButton(n, ui.lfoTab == i, 52)) ui.lfoTab = i;
        }
        ImGui::SameLine(0, 10);
        ImGui::SetCursorPosY(4);
        w::ModSourceHandle(ui, (ModSrc)((int)ModSrc::Lfo1 + ui.lfoTab), 70);
        ImGui::SetCursorPos(ImVec2(10, 36));

        int l = ui.lfoTab;
        // shape buttons
        int shape = (int)ui.s.params.value(lfoParam(l, P::LFO1_SHAPE));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int s = 0; s < 8; ++s) {
            if (s) ImGui::SameLine(0, 4);
            ImGui::PushID(s);
            ImVec2 a = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##shape", ImVec2(34, 22))) {
                ui.s.params.setValue(lfoParam(l, P::LFO1_SHAPE), (float)s);
                ui.presetDirty = true;
            }
            bool hov = ImGui::IsItemHovered();
            dl->AddRectFilled(a, ImVec2(a.x + 34, a.y + 22), s == shape ? theme::modFaint : (hov ? theme::rgba(33, 33, 34) : theme::rgba(26, 26, 27)), 4.0f);
            if (s == shape) dl->AddRect(a, ImVec2(a.x + 34, a.y + 22), theme::modDim, 4.0f);
            views::LfoShapeIcon(ui, l, dl, s, ImVec2(a.x + 6, a.y + 4), ImVec2(a.x + 28, a.y + 18), s == shape ? theme::mod : theme::textDim);
            if (hov) ImGui::SetTooltip("%s", kLfoShapes[s]);
            ImGui::PopID();
        }
        ImGui::SameLine(0, 10);
        w::Choice(ui, lfoParam(l, P::LFO1_MODE), cw - 8 * 38 - 10);

        views::LfoGraph(ui, l, ImVec2(cw, size.y - 36 - 26 - 6 - 78));
        ImGui::Dummy(ImVec2(0, 2));

        bool sync = ui.s.params.value(lfoParam(l, P::LFO1_SYNC)) > 0.5f;
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, 4));
        w::Toggle(ui, lfoParam(l, P::LFO1_SYNC), "Sync");
        if (sync) w::Choice(ui, lfoParam(l, P::LFO1_DIV), 70);
        ImGui::EndGroup();
        ImGui::SameLine(0, 8);
        if (!sync) { w::Knob(ui, lfoParam(l, P::LFO1_RATE), nullptr, 17.0f); ImGui::SameLine(0, 4); }
        knobRow(ui, { lfoParam(l, P::LFO1_PHASE), lfoParam(l, P::LFO1_RISE), lfoParam(l, P::LFO1_SMOOTH) }, 17.0f);
    }
    w::PanelEnd();
}

static void macroPanel(UiState& ui, ImVec2 size)
{
    if (w::PanelBegin(ui, "macros", "MACROS & ARP", size)) {
        float cw = size.x - 20;
        for (int m = 0; m < 4; ++m) {
            if (m) ImGui::SameLine(0, 6);
            ImGui::BeginGroup();
            w::Knob(ui, (P)((int)P::MACRO1 + m), nullptr, 18.0f);
            w::ModSourceHandle(ui, (ModSrc)((int)ModSrc::Macro1 + m), 54);
            ImGui::EndGroup();
        }
        ImGui::Dummy(ImVec2(0, 4));
        w::SectionLabel("Performance sources (drag onto any knob)");
        const ModSrc perf[] = { ModSrc::Velocity, ModSrc::Note, ModSrc::ModWheel, ModSrc::Aftertouch, ModSrc::PitchBend, ModSrc::Random };
        for (int i = 0; i < 6; ++i) {
            if (i % 3) ImGui::SameLine(0, 6);
            w::ModSourceHandle(ui, perf[i], (cw - 12) / 3.0f);
        }

        // ---- arpeggiator ----
        ImGui::Dummy(ImVec2(0, 6));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 sep = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sep.x, sep.y), ImVec2(sep.x + cw, sep.y), theme::border);
        ImGui::Dummy(ImVec2(0, 4));
        w::Power(ui, P::ARP_ON, 15.0f);
        ImGui::SameLine(0, 8);
        ImGui::PushFont(theme::fontBold);
        bool arpOn = ui.s.params.value(P::ARP_ON) > 0.5f;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(arpOn ? theme::text : theme::textFaint), "ARPEGGIATOR");
        ImGui::PopFont();
        int playing = ui.s.telemetry.arpNote.load(std::memory_order_relaxed);
        if (arpOn && playing >= 0) {
            static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            ImGui::SameLine(0, 12);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::accent), "%s%d", names[playing % 12], playing / 12 - 1);
        }
        w::Choice(ui, P::ARP_MODE, (cw - 6) * 0.5f); ImGui::SameLine(0, 6);
        w::Choice(ui, P::ARP_DIV, (cw - 6) * 0.5f);
        w::Stepper(ui, P::ARP_OCT, (cw - 6) * 0.5f, "Octaves"); ImGui::SameLine(0, 10);
        w::Toggle(ui, P::ARP_HOLD, "Hold");
        knobRow(ui, { P::ARP_GATE, P::ARP_SWING, P::TEMPO }, 16.0f);
    }
    w::PanelEnd();
}

static void synthPage(UiState& ui, ImVec2 avail)
{
    const float gap = 8.0f;
    float row1 = std::max(360.0f, avail.y * 0.52f);
    float row2 = avail.y - row1 - gap;
    float oscW = std::floor((avail.x - gap * 3) * 0.29f);
    float subW = std::floor((avail.x - gap * 3) * 0.13f);
    float fltW = avail.x - gap * 3 - oscW * 2 - subW;

    oscPanel(ui, 0, ImVec2(oscW, row1)); ImGui::SameLine(0, gap);
    oscPanel(ui, 1, ImVec2(oscW, row1)); ImGui::SameLine(0, gap);
    ImGui::BeginGroup(); subNoisePanels(ui, ImVec2(subW, row1)); ImGui::EndGroup(); ImGui::SameLine(0, gap);
    filterPanel(ui, ImVec2(fltW, row1));

    ImGui::Dummy(ImVec2(0, gap - ImGui::GetStyle().ItemSpacing.y));
    float envW = std::floor((avail.x - gap * 2) * 0.36f);
    float lfoW = envW;
    float macW = avail.x - gap * 2 - envW - lfoW;
    envPanel(ui, ImVec2(envW, row2)); ImGui::SameLine(0, gap);
    lfoPanel(ui, ImVec2(lfoW, row2)); ImGui::SameLine(0, gap);
    macroPanel(ui, ImVec2(macW, row2));
}

// ---------------------------------------------------------------------
// Matrix page
// ---------------------------------------------------------------------
static bool targetCombo(const char* id, int& target, float width)
{
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    const char* preview = target >= 0 ? paramInfo(target).key : "- target -";
    if (ImGui::BeginCombo(id, preview, ImGuiComboFlags_HeightLargest)) {
        auto group = [&](const char* title, int first, int last) {
            ImGui::SeparatorText(title);
            for (int i = first; i <= last; ++i) {
                const ParamInfo& p = paramInfo(i);
                if (p.map == Map::Bool) continue;
                char label[64];
                std::snprintf(label, sizeof label, "%s##%d", p.label, i);
                if (ImGui::Selectable(label, i == target)) { target = i; changed = true; }
            }
        };
        group("Osc A", (int)P::OSCA_ON, (int)P::OSCA_FILTER);
        group("Osc B", (int)P::OSCB_ON, (int)P::OSCB_FILTER);
        group("Sub / Noise", (int)P::SUB_ON, (int)P::NOISE_FILTER);
        group("Filter 1", (int)P::FLT_ON, (int)P::FLT_MIX);
        group("Filter 2", (int)P::FLT2_ON, (int)P::FLT2_MIX);
        group("Env 1", (int)P::ENV1_ATK, (int)P::ENV1_RCRV);
        group("Env 2", (int)P::ENV2_ATK, (int)P::ENV2_RCRV);
        group("Env 3", (int)P::ENV3_ATK, (int)P::ENV3_RCRV);
        group("LFO 1", (int)P::LFO1_SHAPE, (int)P::LFO1_SMOOTH);
        group("LFO 2", (int)P::LFO2_SHAPE, (int)P::LFO2_SMOOTH);
        group("LFO 3", (int)P::LFO3_SHAPE, (int)P::LFO3_SMOOTH);
        group("LFO 4", (int)P::LFO4_SHAPE, (int)P::LFO4_SMOOTH);
        group("Voice", (int)P::VEL_SENS, (int)P::GLIDE);
        group("FX & Master", (int)P::DIST_MODE, (int)P::MASTER_VOL);
        ImGui::EndCombo();
    }
    return changed;
}

static bool srcCombo(const char* id, ModSrc& src, float width, bool allowNone)
{
    bool changed = false;
    ImGui::SetNextItemWidth(width);
    if (ImGui::BeginCombo(id, modSrcName(src))) {
        for (int i = allowNone ? 0 : 1; i < NUM_MOD_SRC; ++i)
            if (ImGui::Selectable(modSrcName((ModSrc)i), (int)src == i)) { src = (ModSrc)i; changed = true; }
        ImGui::EndCombo();
    }
    return changed;
}

static void matrixPage(UiState& ui, ImVec2 avail)
{
    float tableW = avail.x - 300 - 8;
    if (w::PanelBegin(ui, "matrix", "MODULATION MATRIX", ImVec2(tableW, avail.y))) {
        ImGui::PushFont(theme::fontSmall);
        ImGui::TextDisabled("Each slot adds Source x Amount (x Aux) to the target's knob position. "
                            "Tip: drag a source pill onto any knob on the Synth page.");
        ImGui::PopFont();
        if (ImGui::Button("+ Add slot")) {
            int s = ui.s.mods.firstFree();
            if (s >= 0) { ModSlotData d; d.src = ModSrc::Lfo1; d.target = (int)P::FLT_CUTOFF; d.amount = 0.25f; ui.s.mods.set(s, d); ui.presetDirty = true; }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear all")) { ui.s.mods.clearAll(); ui.presetDirty = true; }

        ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("slots", 8, tf, ImVec2(0, avail.y - 100))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Amount", ImGuiTableColumnFlags_WidthStretch, 1.4f);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 1.3f);
            ImGui::TableSetupColumn("Aux", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 0.8f);
            ImGui::TableSetupColumn("Curve", ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26);
            ImGui::TableHeadersRow();

            for (int i = 0; i < NUM_MOD_SLOTS; ++i) {
                ModSlotData d = ui.s.mods.get(i);
                if (!d.active()) continue;
                ImGui::PushID(i);
                ImGui::TableNextRow(0, 30);
                bool changed = false;

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(i == ui.lastTouchedSlot ? theme::mod : theme::textFaint), "%d", i + 1);

                ImGui::TableNextColumn();
                changed |= srcCombo("##src", d.src, -1, false);

                ImGui::TableNextColumn();
                {
                    float pct = d.amount * 100.0f;
                    ImGui::SetNextItemWidth(-1);
                    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImGui::ColorConvertU32ToFloat4(theme::mod));
                    if (ImGui::SliderFloat("##amt", &pct, -100.0f, 100.0f, "%+.0f%%")) { d.amount = pct / 100.0f; changed = true; }
                    ImGui::PopStyleColor();
                    // live source value bar
                    ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                    float v = ui.s.telemetry.srcValue[(int)d.src].load(std::memory_order_relaxed);
                    float cx = (mn.x + mx.x) * 0.5f, half = (mx.x - mn.x) * 0.5f;
                    float x1 = cx + std::clamp(v * d.amount, -1.0f, 1.0f) * half;
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(std::min(cx, x1), mx.y - 1), ImVec2(std::max(cx, x1), mx.y - 1), theme::mod, 2.0f);
                }

                ImGui::TableNextColumn();
                changed |= targetCombo("##tgt", d.target, -1);

                ImGui::TableNextColumn();
                changed |= srcCombo("##aux", d.aux, -1, true);

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                int mode = (int)d.mode;
                if (ImGui::Combo("##mode", &mode, "Add\0Multiply\0")) { d.mode = (ModMode)mode; changed = true; }

                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                int curve = (int)d.curve;
                if (ImGui::Combo("##curve", &curve, "Linear\0Exp\0Soft\0Hard\0")) { d.curve = (ModCurve)curve; changed = true; }

                ImGui::TableNextColumn();
                if (ImGui::Button("x")) { ui.s.mods.clear(i); ui.presetDirty = true; }
                else if (changed) { ui.s.mods.set(i, d); ui.lastTouchedSlot = i; ui.presetDirty = true; }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    w::PanelEnd();

    ImGui::SameLine(0, 8);
    if (w::PanelBegin(ui, "srcmon", "SOURCE MONITOR", ImVec2(300, avail.y))) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int i = 1; i < NUM_MOD_SRC; ++i) {
            ModSrc s = (ModSrc)i;
            float v = ui.s.telemetry.srcValue[i].load(std::memory_order_relaxed);
            w::ModSourceHandle(ui, s, 84);
            ImGui::SameLine(0, 8);
            ImVec2 a = ImGui::GetCursorScreenPos();
            float bw = 170, bh = 20;
            ImGui::Dummy(ImVec2(bw, bh));
            dl->AddRectFilled(a, ImVec2(a.x + bw, a.y + bh), theme::rgba(16, 16, 17), 3.0f);
            bool bip = modSrcBipolar(s);
            float x0 = bip ? a.x + bw * 0.5f : a.x;
            float x1 = bip ? a.x + bw * 0.5f * (1.0f + std::clamp(v, -1.0f, 1.0f)) : a.x + bw * std::clamp(v, 0.0f, 1.0f);
            dl->AddRectFilled(ImVec2(std::min(x0, x1), a.y + 3), ImVec2(std::max(x0, x1), a.y + bh - 3), theme::modDim, 2.0f);
        }
    }
    w::PanelEnd();
}

// ---------------------------------------------------------------------
// FX page
// ---------------------------------------------------------------------
static void vizFrame(ImDrawList* dl, ImVec2 a, ImVec2 b)
{
    dl->AddRectFilled(a, b, theme::rgba(14, 14, 15), 5.0f);
    dl->AddRect(a, b, theme::rgba(33, 33, 34), 5.0f);
}

// ---------------------------------------------------------------------
// Global page
// ---------------------------------------------------------------------
static void globalPage(UiState& ui, ImVec2 avail)
{
    const float gap = 8.0f;
    float colW = std::floor((avail.x - gap * 2) / 3.0f);

    if (w::PanelBegin(ui, "voicing", "VOICING", ImVec2(colW, avail.y))) {
        w::SectionLabel("Voice mode");
        w::Choice(ui, P::VOICE_MODE, 140);
        w::Stepper(ui, P::POLYPHONY, 200, "Polyphony");
        w::Stepper(ui, P::PB_RANGE, 200, "Pitch bend range");
        ImGui::Dummy(ImVec2(0, 8));
        knobRow(ui, { P::GLIDE, P::VEL_SENS, P::TEMPO }, 22.0f);
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::PushFont(theme::fontSmall);
        ImGui::TextWrapped("Mono retriggers each note; Legato glides between overlapping notes without retriggering. "
                           "Tempo drives synced LFOs and the delay.");
        ImGui::PopFont();
    }
    w::PanelEnd();
    ImGui::SameLine(0, gap);

    if (w::PanelBegin(ui, "io", "AUDIO & MIDI", ImVec2(colW, avail.y))) {
        w::SectionLabel("Audio output");
        if (ui.audio.running()) {
            ImGui::Text("%s", ui.audio.deviceName().c_str());
            float sr = (float)std::max(1u, ui.audio.sampleRate());
            ImGui::TextDisabled("%s, %u Hz, period %u frames (%.1f ms)", ui.audio.backendName().c_str(), ui.audio.sampleRate(),
                                ui.audio.periodFrames(), 1000.0f * ui.audio.periodFrames() / sr);
            ImGui::TextDisabled("Output buffer %u frames = %.1f ms latency", ui.audio.outputBufferFrames(),
                                1000.0f * ui.audio.outputBufferFrames() / sr);
        } else {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::hot), "Audio not running: %s", ui.audio.error().c_str());
        }
        {
            static const int periods[] = { 3, 5, 10, 20 };
            int cur = ui.audio.requestedPeriodMs();
            char prev[32];
            std::snprintf(prev, sizeof prev, "%d ms buffer", cur);
            ImGui::SetNextItemWidth(140);
            if (ImGui::BeginCombo("##period", prev)) {
                for (int p : periods) {
                    char lab[32];
                    std::snprintf(lab, sizeof lab, "%d ms buffer", p);
                    if (ImGui::Selectable(lab, p == cur)) ui.audio.restart(p, ui.audio.exclusiveMode());
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            bool excl = ui.audio.exclusiveMode();
            if (ImGui::Checkbox("Exclusive mode", &excl)) ui.audio.restart(ui.audio.requestedPeriodMs(), excl);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("WASAPI exclusive: lowest latency, but other apps can't play audio meanwhile");
            if (!ui.audio.error().empty()) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::warn), "%s", ui.audio.error().c_str());
        }
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextDisabled("Engine load %.1f%%  (audio callback time / buffer time)", ui.s.telemetry.cpuLoad.load() * 100.0f);
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 a = ImGui::GetCursorScreenPos(), sz(colW - 20, 60);
            ImGui::Dummy(sz);
            vizFrame(dl, a, ImVec2(a.x + sz.x, a.y + sz.y));
            ImVec2 pts[240];
            float peak = 0.0f;
            for (int i = 0; i < 240; ++i) {
                float v = ui.cpuHistory[(ui.cpuHistoryPos + i) % 240];
                peak = std::max(peak, v);
                pts[i] = ImVec2(a.x + sz.x * i / 239.0f, a.y + sz.y - 4 - std::min(v / 0.25f, 1.0f) * (sz.y - 8));
            }
            dl->AddPolyline(pts, 240, theme::accent, 0, 1.5f);
            ImGui::PushFont(theme::fontSmall);
            char b[48]; std::snprintf(b, sizeof b, "peak %.1f%%  (scale 0-25%%)", peak * 100.0f);
            dl->AddText(ImVec2(a.x + 6, a.y + 3), theme::textDim, b);
            ImGui::PopFont();
        }
        ImGui::Dummy(ImVec2(0, 8));

        w::SectionLabel("MIDI input");
        static std::vector<std::string> ports = ui.midi.ports();
        ImGui::SetNextItemWidth(colW - 90);
        const char* cur = ui.midi.openPort() >= 0 ? ui.midi.openPortName().c_str() : "None";
        if (ImGui::BeginCombo("##midiport", cur)) {
            if (ImGui::Selectable("None", ui.midi.openPort() < 0)) ui.midi.close();
            for (int i = 0; i < (int)ports.size(); ++i)
                if (ImGui::Selectable(ports[i].c_str(), i == ui.midi.openPort())) ui.midi.open(i);
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rescan")) ports = ui.midi.ports();
        ImGui::Dummy(ImVec2(0, 8));

        w::SectionLabel("MIDI learn (right-click any knob)");
        int shown = 0;
        for (int cc = 0; cc < 128; ++cc) {
            int p = ui.midi.mappedParam(cc);
            if (p < 0) continue;
            ImGui::PushID(cc);
            ImGui::Text("CC %3d  ->  %s", cc, paramInfo(p).key);
            ImGui::SameLine();
            if (ImGui::SmallButton("clear")) ui.midi.clearMapping(p);
            ImGui::PopID();
            ++shown;
        }
        if (!shown) ImGui::TextDisabled("No mappings yet.");
        if (ui.midi.learning() >= 0) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::warn), "Move a controller to map %s...", paramInfo(ui.midi.learning()).label);
    }
    w::PanelEnd();
    ImGui::SameLine(0, gap);

    if (w::PanelBegin(ui, "help", "PLAYING & SHORTCUTS", ImVec2(colW, avail.y))) {
        ImGui::PushFont(theme::fontSmall);
        auto row = [](const char* k, const char* v) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::accent), "%-14s", k);
            ImGui::SameLine(110);
            ImGui::TextUnformatted(v);
        };
        row("A W S E D ...", "Play notes (piano layout, A = C)");
        row("Z / X", "Octave down / up");
        row("C / V", "Velocity down / up");
        row("Mouse on keys", "Play; higher on the key = softer");
        row("Knob drag", "Change value (Shift = fine)");
        row("Double-click", "Reset to default");
        row("Right-click", "MIDI learn, modulation list");
        row("Drag a source", "Drop on a knob to modulate it");
        row("Alt + drag", "Adjust modulation depth");
        row("3D wavetable", "Drag to rotate, double-click 2D/3D");
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::TextWrapped("MIDI: notes, velocity, pitch bend, mod wheel (CC1), sustain (CC64), "
                           "aftertouch, program change (selects factory presets), all notes off.");
        ImGui::Dummy(ImVec2(0, 8));
        ImGui::TextDisabled("User presets: %s", userPresetDir().c_str());
        ImGui::PopFont();
    }
    w::PanelEnd();
}

// ---------------------------------------------------------------------
// Footer: wheels, piano, keyboard info
// ---------------------------------------------------------------------
static void drawFooter(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 o = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(o, ImVec2(o.x + size.x, o.y + size.y), theme::rgba(15, 15, 16));

    ImGui::SetCursorScreenPos(ImVec2(o.x + 10, o.y + 8));
    views::Wheels(ui, ImVec2(56, size.y - 14));

    float infoW = 170;
    ImGui::SetCursorScreenPos(ImVec2(o.x + 76, o.y + 8));
    views::Piano(ui, ImVec2(size.x - 76 - infoW - 16, size.y - 16), 24, 96);

    ImGui::SetCursorScreenPos(ImVec2(o.x + size.x - infoW - 6, o.y + 10));
    ImGui::BeginGroup();
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::textDim), "KEYBOARD  C%d", ui.keyOctave);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::textDim), "VELOCITY  %d", ui.keyVelocity);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::textFaint), "Z/X octave, C/V velocity");
    if (ImGui::GetTime() < ui.statusUntil) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + infoW);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme::accent), "%s", ui.status.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::PopFont();
    ImGui::EndGroup();
}

// ---------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------
void frame(UiState& ui, bool focused)
{
    ui.wt.collect(ui.audio.running());
    ui.cpuHistory[ui.cpuHistoryPos] = ui.s.telemetry.cpuLoad.load(std::memory_order_relaxed);
    ui.cpuHistoryPos = (ui.cpuHistoryPos + 1) % 240;

    int prog = ui.midi.takeProgramChange();
    if (prog >= 0 && prog < (int)ui.factory.size()) {
        loadPreset(ui, prog);
        ui.setStatus("Program change: " + ui.presetName, ImGui::GetTime());
    }

    if (!focused) releaseKeys(ui);

    {   // undo / redo
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift) ui.undo.undo(ui);
            else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) || (ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyShift)) ui.undo.redo(ui);
        }
        ui.undo.update(ui, ImGui::IsMouseDown(ImGuiMouseButton_Left), ImGui::GetTime());
    }

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("modular_synth", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImVec2 size = vp->WorkSize;
    drawHeader(ui, size.x);

    const float footerH = 112.0f, margin = 10.0f;
    ImVec2 bodyPos(vp->WorkPos.x + margin, ImGui::GetCursorScreenPos().y);
    ImVec2 body(size.x - margin * 2, vp->WorkPos.y + size.y - footerH - 8 - bodyPos.y);
    ImGui::SetCursorScreenPos(bodyPos);
    ImGui::BeginGroup();
    switch (ui.page) {
        case UiState::PageSynth:  synthPage(ui, body); break;
        case UiState::PageMatrix: matrixPage(ui, body); break;
        case UiState::PageFx:     fxpage::draw(ui, body); break;
        default:                  globalPage(ui, body); break;
    }
    ImGui::EndGroup();

    ImGui::SetCursorScreenPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + size.y - footerH));
    drawFooter(ui, ImVec2(size.x, footerH));
    ImGui::End();

    if (ui.wtEditorOpen) wtedit::draw(ui);
}

} // namespace app
