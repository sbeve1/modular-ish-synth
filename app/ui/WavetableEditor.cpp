#include "ui/WavetableEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "imgui.h"
#include "ui/Theme.h"
#include "ui/Views.h"

namespace wtedit {

namespace {
struct EditorState {
    int osc = -1;
    const WavetableSet* source = nullptr;   // table the edit buffer came from
    std::shared_ptr<WavetableSet> edit;     // working copy (frames only)
    std::shared_ptr<WavetableSet> preview;  // built copy for the 3D preview
    std::vector<float> undo;
    std::vector<float> clipboard;
    int frame = 0;
    float pulseWidth = 0.25f;
    float harmonics[32] = { 1.0f };
    bool drawing = false;
    float lastX = -1.0f, lastY = 0.0f;
    bool previewDirty = true;
};
EditorState E;

void pushUndo() { E.undo = E.edit->frames; }

// Build and send to the oscillator (and refresh the preview).
void commit(UiState& ui)
{
    auto built = std::make_shared<WavetableSet>();
    built->name = E.edit->name;
    built->frames = E.edit->frames;
    built->build();
    E.edit->frames = built->frames;   // keep the normalized, DC-free version
    E.preview = built;
    E.source = built.get();
    ui.wt.set(E.osc, built);
    ui.presetDirty = true;
}

void markCustom()
{
    if (findFactoryWavetable(E.edit->name) >= 0 || E.edit->name.empty())
        E.edit->name = "Custom";
}

void beginEdit(UiState& ui, int osc)
{
    E.osc = osc;
    const auto& cur = ui.wt.get(osc);
    E.edit = std::make_shared<WavetableSet>();
    if (cur) { E.edit->frames = cur->frames; E.edit->name = cur->name; }
    E.source = cur.get();
    E.preview = cur;
    E.frame = std::clamp(E.frame, 0, WT_FRAMES - 1);
    E.undo.clear();
}

float* curFrame() { return E.edit->frame(E.frame); }

// ---- drawing canvas ----
void canvas(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##canvas", size);
    bool active = ImGui::IsItemActive();
    float mid = (a.y + b.y) * 0.5f, amp = size.y * 0.45f;

    if (ImGui::IsItemActivated()) { pushUndo(); E.drawing = true; E.lastX = -1.0f; }
    if (active) {
        float x = std::clamp((io.MousePos.x - a.x) / size.x, 0.0f, 0.99999f);
        float y = std::clamp((mid - io.MousePos.y) / amp, -1.0f, 1.0f);
        float* f = curFrame();
        int i1 = (int)(x * WT_SIZE);
        if (E.lastX < 0.0f) { f[i1] = y; }
        else {
            int i0 = (int)(E.lastX * WT_SIZE);
            int lo = std::min(i0, i1), hi = std::max(i0, i1);
            for (int i = lo; i <= hi; ++i) {
                float t = hi == lo ? 1.0f : (float)(i - lo) / (hi - lo);
                float y0 = i0 <= i1 ? E.lastY : y, y1 = i0 <= i1 ? y : E.lastY;
                f[i] = y0 + (y1 - y0) * t;
            }
        }
        E.lastX = x;
        E.lastY = y;
        markCustom();
    }
    if (ImGui::IsItemDeactivated() && E.drawing) { E.drawing = false; commit(ui); }

    dl->AddRectFilled(a, b, theme::rgba(13, 16, 23), 6.0f);
    for (int g = 1; g < 8; ++g) {
        float x = a.x + size.x * g / 8;
        dl->AddLine(ImVec2(x, a.y), ImVec2(x, b.y), theme::rgba(24, 29, 39));
    }
    dl->AddLine(ImVec2(a.x, mid), ImVec2(b.x, mid), theme::rgba(40, 48, 64));
    dl->AddLine(ImVec2(a.x, mid - amp), ImVec2(b.x, mid - amp), theme::rgba(24, 29, 39));
    dl->AddLine(ImVec2(a.x, mid + amp), ImVec2(b.x, mid + amp), theme::rgba(24, 29, 39));

    // neighbours, faint
    for (int nf : { E.frame - 1, E.frame + 1 }) {
        if (nf < 0 || nf >= WT_FRAMES) continue;
        const float* f = E.edit->frame(nf);
        ImVec2 pts[256];
        for (int i = 0; i < 256; ++i) pts[i] = ImVec2(a.x + size.x * i / 255.0f, mid - f[i * WT_SIZE / 256] * amp);
        dl->AddPolyline(pts, 256, theme::withAlpha(theme::accent, 0.12f), 0, 1.0f);
    }
    const float* f = curFrame();
    static ImVec2 pts[512];
    for (int i = 0; i < 512; ++i) pts[i] = ImVec2(a.x + size.x * i / 511.0f, mid - f[i * WT_SIZE / 512] * amp);
    const ImDrawListFlags savedFlags = dl->Flags;
    dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    for (int i = 1; i < 512; ++i)
        dl->AddQuadFilled(pts[i - 1], pts[i], ImVec2(pts[i].x, mid), ImVec2(pts[i - 1].x, mid), theme::withAlpha(theme::accent, 0.08f));
    dl->Flags = savedFlags;
    dl->AddPolyline(pts, 512, theme::withAlpha(theme::accent, 0.25f), 0, 5.0f);
    dl->AddPolyline(pts, 512, theme::accent, 0, 2.0f);
    dl->AddRect(a, b, theme::border, 6.0f);

    ImGui::PushFont(theme::fontSmall);
    char buf[48];
    std::snprintf(buf, sizeof buf, "FRAME %d  -  draw with the mouse", E.frame + 1);
    dl->AddText(ImVec2(a.x + 8, a.y + 6), theme::textDim, buf);
    ImGui::PopFont();
}

void thumbnails(UiState& ui, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float h = 44.0f;
    for (int f = 0; f < WT_FRAMES; ++f) {
        ImGui::PushID(f);
        ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + width, a.y + h);
        if (ImGui::InvisibleButton("##th", ImVec2(width, h))) E.frame = f;
        bool sel = f == E.frame, hov = ImGui::IsItemHovered();
        dl->AddRectFilled(a, b, sel ? theme::rgba(30, 44, 52) : (hov ? theme::rgba(26, 31, 42) : theme::rgba(17, 20, 28)), 4.0f);
        if (sel) dl->AddRect(a, b, theme::accentDim, 4.0f, 0, 1.5f);
        const float* fr = E.edit->frame(f);
        ImVec2 pts[64];
        for (int i = 0; i < 64; ++i) pts[i] = ImVec2(a.x + 22 + (width - 28) * i / 63.0f, (a.y + b.y) * 0.5f - fr[i * WT_SIZE / 64] * h * 0.38f);
        dl->AddPolyline(pts, 64, sel ? theme::accent : theme::textDim, 0, 1.3f);
        char n[4]; std::snprintf(n, sizeof n, "%d", f + 1);
        dl->AddText(ImVec2(a.x + 6, a.y + 4), sel ? theme::accent : theme::textFaint, n);
        ImGui::PopID();
    }
}

void harmonicEditor(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##harm", size);
    if (ImGui::IsItemActive()) {
        ImVec2 m = ImGui::GetIO().MousePos;
        int k = std::clamp((int)((m.x - a.x) / size.x * 32), 0, 31);
        E.harmonics[k] = std::clamp((b.y - m.y) / size.y, 0.0f, 1.0f);
    }
    dl->AddRectFilled(a, b, theme::rgba(13, 16, 23), 4.0f);
    float bw = size.x / 32;
    for (int k = 0; k < 32; ++k) {
        float hgt = E.harmonics[k] * size.y;
        dl->AddRectFilled(ImVec2(a.x + k * bw + 1, b.y - hgt), ImVec2(a.x + (k + 1) * bw - 1, b.y), k == 0 ? theme::accent : theme::accentDim, 1.0f);
    }
}
} // namespace

void draw(UiState& ui)
{
    if (E.osc != ui.wtEditorOsc || !E.edit || E.source != ui.wt.get(ui.wtEditorOsc).get())
        beginEdit(ui, ui.wtEditorOsc);

    ImGui::SetNextWindowSize(ImVec2(1040, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::ColorConvertU32ToFloat4(theme::rgba(18, 21, 30, 252)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    char title[64];
    std::snprintf(title, sizeof title, "Wavetable Editor - Osc %c###wtedit", ui.wtEditorOsc == 0 ? 'A' : 'B');
    if (!ImGui::Begin(title, &ui.wtEditorOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
        return;
    }

    // osc switch + name
    for (int o = 0; o < 2; ++o) {
        if (o) ImGui::SameLine();
        if (ImGui::RadioButton(o == 0 ? "Osc A" : "Osc B", ui.wtEditorOsc == o)) ui.wtEditorOsc = o;
    }
    ImGui::SameLine(0, 20);
    char name[64];
    std::snprintf(name, sizeof name, "%s", E.edit->name.c_str());
    ImGui::SetNextItemWidth(180);
    if (ImGui::InputText("Name", name, sizeof name, ImGuiInputTextFlags_EnterReturnsTrue)) {
        E.edit->name = name;
        markCustom();
        commit(ui);
    }
    ImGui::SameLine(0, 20);
    ImGui::SetNextItemWidth(170);
    if (ImGui::BeginCombo("##load", "Start from bank...")) {
        for (int i = 0; i < numFactoryWavetables(); ++i)
            if (ImGui::Selectable(factoryWavetableName(i))) {
                pushUndo();
                auto t = makeFactoryWavetable(i);
                E.edit->frames = t->frames;
                E.edit->name = t->name;
                commit(ui);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(E.undo.empty());
    if (ImGui::Button("Undo")) { std::swap(E.undo, E.edit->frames); markCustom(); commit(ui); }
    ImGui::EndDisabled();

    ImGui::Separator();

    float avail = ImGui::GetContentRegionAvail().x;
    float thumbW = 120, toolsW = 250;
    float centerW = avail - thumbW - toolsW - 24;

    ImGui::BeginGroup();
    thumbnails(ui, thumbW);
    ImGui::EndGroup();
    ImGui::SameLine(0, 12);

    ImGui::BeginGroup();
    canvas(ui, ImVec2(centerW, 250));
    ImGui::Dummy(ImVec2(0, 4));
    views::WavetableView(ui, E.osc, ImVec2(centerW, 200), E.preview.get(),
                         (float)E.frame / (WT_FRAMES - 1));
    ImGui::EndGroup();
    ImGui::SameLine(0, 12);

    ImGui::BeginGroup();
    ImGui::PushItemWidth(toolsW);
    auto gen = [&](const char* label, auto fn) {
        if (ImGui::Button(label, ImVec2((toolsW - 8) / 3, 0))) { pushUndo(); fn(curFrame()); markCustom(); commit(ui); }
    };
    ImGui::SeparatorText("Generate frame");
    gen("Sine", wtgen::sine); ImGui::SameLine(0, 4);
    gen("Triangle", wtgen::triangle); ImGui::SameLine(0, 4);
    gen("Saw", wtgen::saw);
    gen("Square", wtgen::square); ImGui::SameLine(0, 4);
    gen("Pulse", [](float* f) { wtgen::pulse(f, E.pulseWidth); }); ImGui::SameLine(0, 4);
    gen("Noise", [](float* f) { wtgen::noise(f, (unsigned)(ImGui::GetTime() * 1000)); wtgen::smooth(f, 2); });
    ImGui::SliderFloat("##pw", &E.pulseWidth, 0.02f, 0.5f, "pulse width %.2f");

    ImGui::SeparatorText("Harmonics");
    harmonicEditor(ui, ImVec2(toolsW, 60));
    if (ImGui::Button("Apply harmonics", ImVec2(toolsW, 0))) { pushUndo(); wtgen::additive(curFrame(), E.harmonics, 32); markCustom(); commit(ui); }

    ImGui::SeparatorText("Process frame");
    auto op = [&](const char* label, auto fn) {
        if (ImGui::Button(label, ImVec2((toolsW - 8) / 3, 0))) { pushUndo(); fn(curFrame()); markCustom(); commit(ui); }
    };
    op("Smooth", [](float* f) { wtgen::smooth(f, 6); }); ImGui::SameLine(0, 4);
    op("Normalize", wtgen::normalize); ImGui::SameLine(0, 4);
    op("Invert", [](float* f) { for (int i = 0; i < WT_SIZE; ++i) f[i] = -f[i]; });
    op("Reverse", [](float* f) { std::reverse(f, f + WT_SIZE); }); ImGui::SameLine(0, 4);
    op("Mirror", [](float* f) { for (int i = 0; i < WT_SIZE / 2; ++i) f[WT_SIZE - 1 - i] = -f[i]; }); ImGui::SameLine(0, 4);
    op("Clear", [](float* f) { std::fill(f, f + WT_SIZE, 0.0f); });

    ImGui::SeparatorText("Frames");
    if (ImGui::Button("Copy", ImVec2((toolsW - 4) / 2, 0))) E.clipboard.assign(curFrame(), curFrame() + WT_SIZE);
    ImGui::SameLine(0, 4);
    ImGui::BeginDisabled(E.clipboard.size() != (size_t)WT_SIZE);
    if (ImGui::Button("Paste", ImVec2((toolsW - 4) / 2, 0))) { pushUndo(); std::copy(E.clipboard.begin(), E.clipboard.end(), curFrame()); markCustom(); commit(ui); }
    ImGui::EndDisabled();
    if (ImGui::Button("Morph fill 1 > 8", ImVec2(toolsW, 0))) {
        pushUndo();
        const float* first = E.edit->frame(0);
        const float* last = E.edit->frame(WT_FRAMES - 1);
        for (int f = 1; f < WT_FRAMES - 1; ++f) {
            float t = (float)f / (WT_FRAMES - 1);
            float* d = E.edit->frame(f);
            for (int i = 0; i < WT_SIZE; ++i) d[i] = first[i] + (last[i] - first[i]) * t;
        }
        markCustom();
        commit(ui);
    }
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextWrapped("Frames 2-7 become a crossfade between frames 1 and 8. Edits are band-limited and heard immediately.");
    ImGui::PopFont();
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace wtedit
