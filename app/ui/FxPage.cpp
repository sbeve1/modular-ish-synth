#include "ui/FxPage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/Dsp.h"
#include "engine/Effects.h"
#include "imgui_internal.h"
#include "ui/Theme.h"
#include "ui/Views.h"
#include "ui/Widgets.h"

namespace fxpage {

namespace {

constexpr const char* FX_PAYLOAD = "NW_FXSLOT";

struct NoAAFill {
    ImDrawList* dl; ImDrawListFlags saved;
    explicit NoAAFill(ImDrawList* d) : dl(d), saved(d->Flags) { dl->Flags &= ~ImDrawListFlags_AntiAliasedFill; }
    ~NoAAFill() { dl->Flags = saved; }
};

void vizFrame(ImDrawList* dl, ImVec2 a, ImVec2 b)
{
    dl->AddRectFilled(a, b, theme::rgba(11, 11, 12), 3.0f);
    dl->AddRect(a, b, theme::border, 3.0f);
}

void knobRow(UiState& ui, std::initializer_list<P> params, float radius = 24.0f)
{
    bool first = true;
    for (P p : params) {
        if (!first) ImGui::SameLine(0, 6);
        w::Knob(ui, p, nullptr, radius);
        first = false;
    }
}

float vp(UiState& ui, P p) { return ui.s.params.value(p); }

// log-frequency helpers shared by the response views
struct LogAxis {
    ImVec2 a, b;
    float fMin = 20.0f, fMax = 20000.0f;
    float x(float f) const { return a.x + (b.x - a.x) * std::log(f / fMin) / std::log(fMax / fMin); }
    float f(float x) const { return fMin * std::pow(fMax / fMin, std::clamp((x - a.x) / (b.x - a.x), 0.0f, 1.0f)); }
};

void freqGrid(ImDrawList* dl, const LogAxis& ax)
{
    for (float f : { 100.0f, 1000.0f, 10000.0f }) dl->AddLine(ImVec2(ax.x(f), ax.a.y), ImVec2(ax.x(f), ax.b.y), theme::rgba(30, 30, 33));
    ImGui::PushFont(theme::fontSmall);
    for (auto [f, l] : { std::pair{ 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        dl->AddText(ImVec2(ax.x(f) + 3, ax.b.y - 15), theme::textFaint, l);
    ImGui::PopFont();
}

void curve(ImDrawList* dl, const ImVec2* pts, int n, float baseY, ImU32 col, bool fill)
{
    if (fill) {
        NoAAFill noaa(dl);
        for (int i = 1; i < n; ++i)
            dl->AddQuadFilled(pts[i - 1], pts[i], ImVec2(pts[i].x, baseY), ImVec2(pts[i - 1].x, baseY), theme::withAlpha(col, 0.08f));
    }
    dl->AddPolyline(pts, n, col, 0, 2.0f);
}

// ---------------------------------------------------------------------
// Visualizations
// ---------------------------------------------------------------------
void distViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    dl->AddLine(ImVec2(a.x, (a.y + b.y) * 0.5f), ImVec2(b.x, (a.y + b.y) * 0.5f), theme::rgba(30, 30, 33));
    dl->AddLine(ImVec2((a.x + b.x) * 0.5f, a.y), ImVec2((a.x + b.x) * 0.5f, b.y), theme::rgba(30, 30, 33));
    int mode = (int)vp(ui, P::DIST_MODE);
    float g = dsp::dbToGain(vp(ui, P::DIST_DRIVE) * 36.0f);
    float q = std::exp2(16.0f - vp(ui, P::DIST_DRIVE) * 13.0f - 1.0f);
    const int N = 160;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i) {
        float x = (float)i / (N - 1) * 2.0f - 1.0f, xi = x * g, y;
        switch (mode) {
            case 0: y = dsp::fastTanh(xi); break;
            case 1: y = std::clamp(xi, -1.0f, 1.0f); break;
            case 2: y = xi >= 0 ? dsp::fastTanh(xi) : xi / (1.0f + std::fabs(xi)) * 0.8f; break;
            case 3: y = std::sin(xi * 0.5f * dsp::PI); break;
            default: y = std::round(x * q) / q; break;
        }
        pts[i] = ImVec2(a.x + size.x * (x + 1) * 0.5f, (a.y + b.y) * 0.5f - std::clamp(y, -1.0f, 1.0f) * size.y * 0.44f);
    }
    dl->AddPolyline(pts, N, theme::accent, 0, 2.0f);
}

void eqViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##eq", size);
    bool active = ImGui::IsItemActive(), hovered = ImGui::IsItemHovered();
    vizFrame(dl, a, b);
    LogAxis ax{ a, b };
    const float dbRange = 20.0f;
    auto yOf = [&](float db) { return (a.y + b.y) * 0.5f - std::clamp(db, -dbRange, dbRange) / dbRange * size.y * 0.45f; };
    freqGrid(dl, ax);
    dl->AddLine(ImVec2(a.x, yOf(0)), ImVec2(b.x, yOf(0)), theme::rgba(40, 40, 44));

    float sr = 48000.0f;
    Biquad bands[3];
    bands[0].lowShelf(vp(ui, P::EQ_LOWFREQ), vp(ui, P::EQ_LOW), sr);
    bands[1].peak(vp(ui, P::EQ_MIDFREQ), vp(ui, P::EQ_MIDQ), vp(ui, P::EQ_MID), sr);
    bands[2].highShelf(vp(ui, P::EQ_HIGHFREQ), vp(ui, P::EQ_HIGH), sr);
    const int N = 200;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i) {
        float f = ax.fMin * std::pow(ax.fMax / ax.fMin, (float)i / (N - 1));
        float db = bands[0].magnitudeDb(f, sr) + bands[1].magnitudeDb(f, sr) + bands[2].magnitudeDb(f, sr);
        pts[i] = ImVec2(ax.x(f), yOf(db));
    }
    curve(dl, pts, N, yOf(0), theme::accent, true);

    // band nodes: drag x = frequency, y = gain; wheel on the mid node = Q
    struct Node { P freq, gain; const char* label; };
    const Node nodes[3] = { { P::EQ_LOWFREQ, P::EQ_LOW, "L" }, { P::EQ_MIDFREQ, P::EQ_MID, "M" }, { P::EQ_HIGHFREQ, P::EQ_HIGH, "H" } };
    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID idNode = ImGui::GetID("eqnode");
    auto nodePos = [&](const Node& n) { return ImVec2(ax.x(vp(ui, n.freq)), yOf(vp(ui, n.gain))); };
    if (ImGui::IsItemActivated()) {
        int best = -1; float bd = 18.0f * 18.0f;
        for (int k = 0; k < 3; ++k) {
            ImVec2 p = nodePos(nodes[k]);
            float d = (p.x - io.MousePos.x) * (p.x - io.MousePos.x) + (p.y - io.MousePos.y) * (p.y - io.MousePos.y);
            if (d < bd) { bd = d; best = k; }
        }
        st->SetInt(idNode, best);
    }
    int hot = active ? st->GetInt(idNode, -1) : -1;
    if (hot >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        ui.s.params.setValue(nodes[hot].freq, ax.f(io.MousePos.x));
        ui.s.params.setValue(nodes[hot].gain, std::clamp(((a.y + b.y) * 0.5f - io.MousePos.y) / (size.y * 0.45f) * dbRange, -18.0f, 18.0f));
        ui.presetDirty = true;
    }
    if (hovered && io.MouseWheel != 0.0f) {
        ImVec2 p = nodePos(nodes[1]);
        if (std::fabs(p.x - io.MousePos.x) < 30.0f)
            ui.s.params.setNorm(P::EQ_MIDQ, ui.s.params.norm(P::EQ_MIDQ) + io.MouseWheel * 0.04f);
    }
    ImGui::PushFont(theme::fontSmall);
    for (int k = 0; k < 3; ++k) {
        ImVec2 p = nodePos(nodes[k]);
        bool h = hot == k;
        dl->AddCircleFilled(p, h ? 9.0f : 7.5f, theme::rgba(11, 11, 12));
        dl->AddCircle(p, h ? 9.0f : 7.5f, theme::accent, 20, 2.0f);
        ImVec2 ts = ImGui::CalcTextSize(nodes[k].label);
        dl->AddText(ImVec2(p.x - ts.x * 0.5f, p.y - ts.y * 0.5f), theme::text, nodes[k].label);
    }
    dl->AddText(ImVec2(a.x + 8, a.y + 6), theme::textDim, "drag nodes: frequency / gain   -   scroll on M: width");
    ImGui::PopFont();
}

void compViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);

    // transfer curve (square area) + gain reduction meter
    float side = std::min(size.y - 30, size.x - 110);
    ImVec2 ca(a.x + (size.x - side - 60) * 0.5f, a.y + 10), cb(ca.x + side, ca.y + side);
    const float lo = -60.0f;
    auto px = [&](float db) { return ca.x + (db - lo) / -lo * side; };
    auto py = [&](float db) { return cb.y - (std::clamp(db, lo, 0.0f) - lo) / -lo * side; };
    for (float g = -48; g < 0; g += 12) {
        dl->AddLine(ImVec2(px(g), ca.y), ImVec2(px(g), cb.y), theme::rgba(26, 26, 29));
        dl->AddLine(ImVec2(ca.x, py(g)), ImVec2(cb.x, py(g)), theme::rgba(26, 26, 29));
    }
    dl->AddRect(ca, cb, theme::rgba(36, 36, 40));
    dl->AddLine(ImVec2(ca.x, cb.y), ImVec2(cb.x, ca.y), theme::rgba(40, 40, 44));

    float th = vp(ui, P::COMP_THRESH), ratio = vp(ui, P::COMP_RATIO), makeup = vp(ui, P::COMP_MAKEUP);
    float slope = 1.0f - 1.0f / ratio, knee = 6.0f;
    const int N = 120;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i) {
        float in = lo + (-lo) * i / (N - 1), over = in - th, gr;
        if (over <= -knee * 0.5f) gr = 0.0f;
        else if (over >= knee * 0.5f) gr = over * slope;
        else { float x = over + knee * 0.5f; gr = slope * x * x / (2.0f * knee); }
        pts[i] = ImVec2(px(in), py(in - gr + makeup));
    }
    dl->AddPolyline(pts, N, theme::accent, 0, 2.0f);
    dl->AddLine(ImVec2(px(th), ca.y), ImVec2(px(th), cb.y), theme::withAlpha(theme::text, 0.25f));

    float grNow = ui.s.telemetry.compGainReduction.load(std::memory_order_relaxed);
    static float grShown = 0.0f;
    grShown = std::max(grNow, grShown * 0.92f);
    ImVec2 ma(cb.x + 30, ca.y), mb(ma.x + 16, cb.y);
    dl->AddRectFilled(ma, mb, theme::rgba(18, 18, 20), 2.0f);
    float t = std::clamp(grShown / 24.0f, 0.0f, 1.0f);
    dl->AddRectFilled(ma, ImVec2(mb.x, ma.y + (mb.y - ma.y) * t), theme::mod, 2.0f);
    ImGui::PushFont(theme::fontSmall);
    char buf[32];
    std::snprintf(buf, sizeof buf, "GR %.1f dB", grShown);
    dl->AddText(ImVec2(ma.x - 12, mb.y + 2), theme::textDim, buf);
    dl->AddText(ImVec2(ca.x + 6, ca.y + 4), theme::textFaint, "in -> out (dB)");
    ImGui::PopFont();
}

// phaser: notches of 6 swept all-passes summed with the dry signal
void phaserViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    LogAxis ax{ a, b };
    freqGrid(dl, ax);
    float lfo = 0.5f + 0.5f * std::sin(dsp::TWO_PI * (float)ImGui::GetTime() * vp(ui, P::PHA_RATE));
    float fc = 200.0f * std::pow(25.0f, lfo * vp(ui, P::PHA_DEPTH));
    float mix = vp(ui, P::PHA_MIX);
    const int N = 220;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i) {
        float f = ax.fMin * std::pow(ax.fMax / ax.fMin, (float)i / (N - 1));
        float phi = -2.0f * std::atan(f / fc) * 6.0f;
        float mag = std::fabs(std::cos(phi * 0.5f));
        float y = (1.0f - mix) + mix * mag;
        pts[i] = ImVec2(ax.x(f), b.y - 8 - y * (size.y - 30));
    }
    curve(dl, pts, N, b.y - 8, theme::accent, true);
}

// flanger: comb filter from the modulated short delay
void flangerViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    LogAxis ax{ a, b };
    freqGrid(dl, ax);
    float m = 0.5f + 0.5f * std::sin(dsp::TWO_PI * (float)ImGui::GetTime() * vp(ui, P::FLG_RATE));
    float tau = 0.0006f + (0.0004f + vp(ui, P::FLG_DEPTH) * 0.0065f) * m;
    float fb = vp(ui, P::FLG_FB), mix = vp(ui, P::FLG_MIX);
    const int N = 260;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i) {
        float f = ax.fMin * std::pow(ax.fMax / ax.fMin, (float)i / (N - 1));
        float w = dsp::TWO_PI * f * tau;
        // H = 1 + e^-jw / (1 - fb e^-jw)
        float dr = 1.0f - fb * std::cos(w), di = fb * std::sin(w);
        float den = dr * dr + di * di;
        float cr = (std::cos(w) * dr - std::sin(w) * di) / den, ci = (-std::sin(w) * dr - std::cos(w) * di) / den;
        float mag = std::sqrt((1.0f + cr) * (1.0f + cr) + ci * ci) * 0.5f;
        float y = std::clamp((1.0f - mix) + mix * mag, 0.0f, 1.6f) / 1.6f;
        pts[i] = ImVec2(ax.x(f), b.y - 8 - y * (size.y - 30));
    }
    curve(dl, pts, N, b.y - 8, theme::accent, true);
}

void chorusViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    float depth = vp(ui, P::CHO_DEPTH), rate = vp(ui, P::CHO_RATE);
    float mid = (a.y + b.y) * 0.5f, amp = size.y * 0.4f * (0.1f + 0.9f * depth);
    float phase0 = (float)ImGui::GetTime() * rate;
    const int N = 160;
    ImVec2 l[N], r[N];
    for (int i = 0; i < N; ++i) {
        float p = (float)i / (N - 1) * 2.0f;
        float x = a.x + 10 + (size.x - 20) * i / (N - 1);
        l[i] = ImVec2(x, mid - std::sin(dsp::TWO_PI * (p + phase0)) * amp);
        r[i] = ImVec2(x, mid - std::sin(dsp::TWO_PI * (p + phase0 + 0.25f)) * amp);
    }
    dl->AddPolyline(l, N, theme::accent, 0, 2.0f);
    dl->AddPolyline(r, N, theme::mod, 0, 2.0f);
    ImGui::PushFont(theme::fontSmall);
    dl->AddText(ImVec2(a.x + 8, a.y + 6), theme::accent, "L delay");
    dl->AddText(ImVec2(a.x + 64, a.y + 6), theme::mod, "R delay");
    ImGui::PopFont();
}

void delayViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    bool sync = vp(ui, P::DLY_SYNC) > 0.5f;
    float tempo = vp(ui, P::TEMPO);
    float t = sync ? (60.0f / tempo) * kSyncDivBeats[std::clamp((int)vp(ui, P::DLY_DIV), 0, 14)] : vp(ui, P::DLY_TIME) * 0.001f;
    float fb = vp(ui, P::DLY_FB), mix = vp(ui, P::DLY_MIX), tone = vp(ui, P::DLY_TONE);
    bool ping = vp(ui, P::DLY_PING) > 0.5f;
    const float window = std::max(2.0f, t * 6.0f);
    float mid = (a.y + b.y) * 0.5f, amp = size.y * 0.42f;
    dl->AddLine(ImVec2(a.x, mid), ImVec2(b.x, mid), theme::rgba(30, 30, 33));
    auto xOf = [&](float sec) { return a.x + 10 + (size.x - 20) * sec / window; };
    for (float beat = 0; beat < window; beat += 60.0f / tempo)
        dl->AddLine(ImVec2(xOf(beat), a.y + 4), ImVec2(xOf(beat), b.y - 4), theme::rgba(22, 22, 25));
    dl->AddLine(ImVec2(xOf(0), mid - amp), ImVec2(xOf(0), mid + amp), theme::accent, 3.0f);
    float g = 0.85f;
    for (int k = 1; k * t < window && g > 0.01f; ++k) {
        float x = xOf(k * t), h = std::min(g, 1.0f) * amp;
        ImU32 col = theme::lerpColor(theme::mod, theme::rgba(90, 90, 96), std::min(1.0f, (k - 1) * (1.0f - tone) * 0.5f));
        if (ping) dl->AddLine(ImVec2(x, mid), ImVec2(x, (k % 2) ? mid - h : mid + h), col, 3.0f);
        else      dl->AddLine(ImVec2(x, mid - h), ImVec2(x, mid + h), col, 3.0f);
        g *= fb;
    }
    ImGui::PushFont(theme::fontSmall);
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.0f ms   mix %.0f%%%s", t * 1000.0f, mix * 100.0f, ping ? "   L above / R below" : "");
    dl->AddText(ImVec2(a.x + 8, a.y + 6), theme::textDim, buf);
    ImGui::PopFont();
}

void reverbViz(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    vizFrame(dl, a, b);
    float decay = vp(ui, P::REV_DECAY), pre = vp(ui, P::REV_PREDLY) * 0.001f;
    float width = vp(ui, P::REV_WIDTH), damp = vp(ui, P::REV_DAMP), size01 = vp(ui, P::REV_SIZE);
    float window = std::max(1.0f, (pre + decay) * 1.1f);
    float mid = (a.y + b.y) * 0.5f, amp = size.y * 0.42f;
    auto xOf = [&](float sec) { return a.x + 10 + (size.x - 20) * sec / window; };
    dl->AddLine(ImVec2(xOf(0), mid - amp), ImVec2(xOf(0), mid + amp), theme::accent, 3.0f);
    uint32_t h = 12345u;
    int n = (int)(size.x - 20);
    for (int i = 0; i < n; ++i) {
        float sec = window * i / n;
        if (sec < pre) continue;
        float env = std::pow(10.0f, -3.0f * (sec - pre) / decay);
        float density = std::min(1.0f, (sec - pre) / (0.02f + 0.06f * size01));
        h = h * 1664525u + 1013904223u;
        float r1 = ((h >> 8) & 0xFFFF) / 65535.0f;
        h = h * 1664525u + 1013904223u;
        float r2 = ((h >> 8) & 0xFFFF) / 65535.0f;
        float up = env * density * (0.35f + 0.65f * r1) * amp;
        float dn = env * density * (0.35f + 0.65f * (width * r2 + (1.0f - width) * r1)) * amp;
        ImU32 col = theme::lerpColor(theme::mod, theme::rgba(80, 80, 86), std::min(1.0f, damp * (sec - pre) / decay * 2.0f));
        float x = a.x + 10 + i;
        dl->AddLine(ImVec2(x, mid - up), ImVec2(x, mid + dn), theme::withAlpha(col, 0.75f));
    }
    ImGui::PushFont(theme::fontSmall);
    char buf[64];
    std::snprintf(buf, sizeof buf, "RT60 %.1f s   pre-delay %.0f ms", decay, pre * 1000.0f);
    dl->AddText(ImVec2(a.x + 8, a.y + 6), theme::textDim, buf);
    ImGui::PopFont();
}

const char* fxDescription(FxId id)
{
    switch (id) {
        case FxId::Distortion: return "Waveshaping with drive, tone filter and DC blocker";
        case FxId::Phaser:     return "6-stage all-pass phaser, stereo LFOs 90 degrees apart";
        case FxId::Flanger:    return "Short modulated delay with positive or negative feedback";
        case FxId::Chorus:     return "Stereo modulated delay, LFOs 90 degrees apart";
        case FxId::Delay:      return "Stereo delay with tone filter in the feedback path";
        case FxId::Reverb:     return "8-line feedback delay network with diffusion and damping";
        case FxId::Eq:         return "Low shelf, mid peak, high shelf";
        default:               return "Stereo-linked compressor with a soft knee";
    }
}

// ---------------------------------------------------------------------
// Rack
// ---------------------------------------------------------------------
void rack(UiState& ui, ImVec2 size)
{
    if (!w::PanelBegin(ui, "rack", "FX RACK", size)) { w::PanelEnd(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    uint32_t order = ui.s.fxOrder.load(std::memory_order_relaxed);
    const float rowH = 34.0f, rowW = size.x - 20;

    for (int slot = 0; slot < NUM_FX; ++slot) {
        FxId id = fxAt(order, slot);
        ImGui::PushID(slot);
        ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + rowW, a.y + rowH);
        bool on = ui.s.params.value(fxPowerParam(id)) > 0.5f;
        bool selected = ui.fxSelected == (int)id;

        // power button sits on top of the row button
        ImGui::SetCursorScreenPos(ImVec2(a.x + rowW - 26, a.y + (rowH - 15) * 0.5f));
        w::Power(ui, fxPowerParam(id));
        ImGui::SetCursorScreenPos(a);
        ImGui::SetNextItemAllowOverlap();
        if (ImGui::InvisibleButton("##row", ImVec2(rowW - 32, rowH))) ui.fxSelected = (int)id;
        bool hov = ImGui::IsItemHovered();

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(FX_PAYLOAD, &slot, sizeof slot);
            ImGui::Text("Move %s", fxName(id));
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(FX_PAYLOAD)) {
                int from = *(const int*)p->Data;
                int ids[NUM_FX];
                for (int i = 0; i < NUM_FX; ++i) ids[i] = (int)fxAt(order, i);
                int moved = ids[from];
                if (from < slot) for (int i = from; i < slot; ++i) ids[i] = ids[i + 1];
                else             for (int i = from; i > slot; --i) ids[i] = ids[i - 1];
                ids[slot] = moved;
                uint32_t next = 0;
                for (int i = 0; i < NUM_FX; ++i) next |= (uint32_t)ids[i] << (i * 4);
                ui.s.fxOrder.store(next, std::memory_order_relaxed);
                ui.presetDirty = true;
            }
            ImGui::EndDragDropTarget();
        }

        dl->AddRectFilled(a, b, selected ? theme::rgba(30, 30, 34) : (hov ? theme::rgba(24, 24, 27) : theme::rgba(18, 18, 20)), 3.0f);
        if (selected) dl->AddRectFilled(a, ImVec2(a.x + 3, b.y), theme::accent, 1.0f);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 2; ++c)
                dl->AddCircleFilled(ImVec2(a.x + 12 + c * 4, a.y + rowH * 0.5f - 4 + r * 4), 1.2f, theme::textFaint);
        char num[4];
        std::snprintf(num, sizeof num, "%d", slot + 1);
        ImGui::PushFont(theme::fontSmall);
        dl->AddText(ImVec2(a.x + 24, a.y + (rowH - ImGui::GetFontSize()) * 0.5f), theme::textFaint, num);
        ImGui::PopFont();
        ImGui::PushFont(theme::fontBold);
        dl->AddText(ImVec2(a.x + 40, a.y + (rowH - ImGui::GetFontSize()) * 0.5f), on ? theme::text : theme::textDim, fxName(id));
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(a.x, b.y + 4));
        ImGui::PopID();
    }
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextWrapped("Signal flows top to bottom. Drag a row to reorder; click to edit.");
    ImGui::PopFont();
    w::PanelEnd();
}

void editor(UiState& ui, ImVec2 size)
{
    FxId id = (FxId)std::clamp(ui.fxSelected, 0, NUM_FX - 1);
    char title[32];
    std::snprintf(title, sizeof title, "%s", fxName(id));
    for (char* c = title; *c; ++c) *c = (char)std::toupper((unsigned char)*c);
    if (!w::PanelBegin(ui, "fxedit", title, size, (int)fxPowerParam(id))) { w::PanelEnd(); return; }

    float cw = size.x - 20;
    ImGui::PushFont(theme::fontSmall);
    ImGui::TextDisabled("%s", fxDescription(id));
    ImGui::PopFont();
    float vizH = size.y - 36 - 24 - 110;

    switch (id) {
        case FxId::Distortion:
            w::Choice(ui, P::DIST_MODE, 120);
            distViz(ui, ImVec2(cw, vizH - 30));
            knobRow(ui, { P::DIST_DRIVE, P::DIST_TONE, P::DIST_MIX });
            break;
        case FxId::Phaser:
            phaserViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::PHA_RATE, P::PHA_DEPTH, P::PHA_FB, P::PHA_MIX });
            break;
        case FxId::Flanger:
            flangerViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::FLG_RATE, P::FLG_DEPTH, P::FLG_FB, P::FLG_MIX });
            break;
        case FxId::Chorus:
            chorusViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::CHO_RATE, P::CHO_DEPTH, P::CHO_FB, P::CHO_MIX });
            break;
        case FxId::Delay: {
            bool sync = vp(ui, P::DLY_SYNC) > 0.5f;
            w::Toggle(ui, P::DLY_SYNC, "Tempo sync"); ImGui::SameLine(0, 16);
            w::Toggle(ui, P::DLY_PING, "Ping-pong");
            delayViz(ui, ImVec2(cw, vizH - 26));
            if (sync) {
                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0, 8));
                w::Choice(ui, P::DLY_DIV, 90);
                ImGui::TextDisabled("%.0f BPM", vp(ui, P::TEMPO));
                ImGui::EndGroup();
                ImGui::SameLine(0, 12);
            } else {
                w::Knob(ui, P::DLY_TIME, nullptr, 24.0f);
                ImGui::SameLine(0, 6);
            }
            knobRow(ui, { P::DLY_FB, P::DLY_TONE, P::DLY_MIX });
            break;
        }
        case FxId::Reverb:
            reverbViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::REV_SIZE, P::REV_DECAY, P::REV_DAMP, P::REV_PREDLY, P::REV_WIDTH, P::REV_MIX });
            break;
        case FxId::Eq:
            eqViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::EQ_LOW, P::EQ_LOWFREQ, P::EQ_MID, P::EQ_MIDFREQ, P::EQ_MIDQ, P::EQ_HIGH, P::EQ_HIGHFREQ }, 21.0f);
            break;
        default:
            compViz(ui, ImVec2(cw, vizH));
            knobRow(ui, { P::COMP_THRESH, P::COMP_RATIO, P::COMP_ATK, P::COMP_REL, P::COMP_MAKEUP, P::COMP_MIX });
            break;
    }
    w::PanelEnd();
}

void output(UiState& ui, ImVec2 size)
{
    if (w::PanelBegin(ui, "out", "OUTPUT", size)) {
        float cw = size.x - 20;
        w::SectionLabel("Spectrum");
        views::Spectrum(ui, ImVec2(cw, 170));
        w::SectionLabel("Oscilloscope");
        views::Oscilloscope(ui, ImVec2(cw, 110));
        ImGui::Dummy(ImVec2(0, 6));
        w::Knob(ui, P::MASTER_VOL, "Master", 26.0f);
        ImGui::SameLine(0, 12);
        w::LevelMeter(ui.peakL, ImVec2(10, 80));
        ImGui::SameLine(0, 3);
        w::LevelMeter(ui.peakR, ImVec2(10, 80));
        ImGui::PushFont(theme::fontSmall);
        ImGui::TextWrapped("Voices > FX rack (in order) > master > limiter");
        ImGui::PopFont();
    }
    w::PanelEnd();
}

} // namespace

void draw(UiState& ui, ImVec2 avail)
{
    const float gap = 8.0f, rackW = 240.0f, outW = 330.0f;
    rack(ui, ImVec2(rackW, avail.y));
    ImGui::SameLine(0, gap);
    editor(ui, ImVec2(avail.x - rackW - outW - gap * 2, avail.y));
    ImGui::SameLine(0, gap);
    output(ui, ImVec2(outW, avail.y));
}

} // namespace fxpage
