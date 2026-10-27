#include "ui/Views.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "engine/Dsp.h"
#include "engine/Lfo.h"
#include "host/AudioHost.h"
#include "imgui_internal.h"
#include "ui/Theme.h"

namespace views {

static float frac01(float x) { return x - std::floor(x); }

// Filled areas are drawn as many thin quads; with AA fringes they overlap and
// look hatched, so fills turn anti-aliasing off.
struct NoAAFill {
    ImDrawList* dl; ImDrawListFlags saved;
    explicit NoAAFill(ImDrawList* d) : dl(d), saved(d->Flags) { dl->Flags &= ~ImDrawListFlags_AntiAliasedFill; }
    ~NoAAFill() { dl->Flags = saved; }
};

static void frameBg(ImDrawList* dl, ImVec2 a, ImVec2 b)
{
    dl->AddRectFilled(a, b, theme::rgba(14, 14, 15), 5.0f);
    dl->AddRect(a, b, theme::rgba(33, 33, 34), 5.0f);
}

// ---------------------------------------------------------------------
// Wavetable view
//
// Top: the 8 frames in an oblique 3D stack (frame 1 at the front), numbered,
// fading with distance from the playing position, which is filled and marked
// on a depth rail. Bottom: a 2D strip of the exact waveform being played,
// with warp applied, like Serum's 2D view.
// ---------------------------------------------------------------------
namespace {
constexpr int PTS = 128;    // per frame in the 3D stack
constexpr int PTS2 = 256;   // in the 2D strip

struct WtCache {
    const WavetableSet* src = nullptr;
    float frames[WT_FRAMES][PTS];
};
WtCache gCache[3];

// zx/zy: screen direction of the depth axis; turn: rotation of the waveform axis
struct WtViewState { float zx = 0.34f, zy = 0.52f, turn = 0.13f; bool flat = false; };
WtViewState gView[3];

void buildCache(WtCache& c, const WavetableSet* wt)
{
    c.src = wt;
    const int level = 4;   // 64 harmonics: smooth to draw
    for (int f = 0; f < WT_FRAMES; ++f) {
        const float* t = wt->mips.empty() ? wt->frame(f) : wt->table(level, f);
        for (int i = 0; i < PTS; ++i) c.frames[f][i] = t[i * WT_SIZE / PTS];
    }
}

// Same phase warps as the oscillator (Voice.cpp), for display.
float warpPhaseUi(float p, int warp, float amt)
{
    switch (warp) {
        case 1: if (std::fabs(amt - 0.5f) > 1e-3f) p = std::pow(p, std::exp2((amt - 0.5f) * 4.0f)); break;
        case 2: { float w = 0.5f - amt * 0.47f; p = p < w ? p * (0.5f / w) : 0.5f + (p - w) * (0.5f / (1.0f - w)); break; }
        case 3: p = frac01(p * (1.0f + amt * 7.0f)); break;
        case 4: p = p + ((p < 0.5f ? p : 1.0f - p) - p) * amt; break;
        default: break;
    }
    return std::clamp(p, 0.0f, 0.99999f);
}

float readTable(const WavetableSet* wt, int level, float pos, float phase)
{
    float fp = pos * (WT_FRAMES - 1);
    int f0 = std::min((int)fp, WT_FRAMES - 1), f1 = std::min(f0 + 1, WT_FRAMES - 1);
    float ff = fp - f0;
    auto at = [&](int f) {
        const float* t = wt->mips.empty() ? wt->frame(f) : wt->table(level, f);
        float idx = phase * WT_SIZE;
        int i0 = (int)idx;
        float fr = idx - i0;
        int i1 = wt->mips.empty() ? (i0 + 1) % WT_SIZE : i0 + 1;
        return t[i0] + (t[i1] - t[i0]) * fr;
    };
    float a = at(f0), b = at(f1);
    return a + (b - a) * ff;
}
} // namespace

void WavetableView(UiState& ui, int osc, ImVec2 size, const WavetableSet* overrideTable, float overridePos)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(osc + 31000);
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##wt", size);
    int vi = overrideTable ? 2 : osc;
    WtViewState& vs = gView[vi];
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f)) {
        vs.turn = std::clamp(vs.turn + ImGui::GetIO().MouseDelta.x * 0.004f, -0.6f, 0.6f);
        vs.zy = std::clamp(vs.zy + ImGui::GetIO().MouseDelta.y * 0.004f, 0.3f, 1.1f);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) vs.flat = !vs.flat;
    if (ImGui::IsItemHovered() && !ImGui::IsItemActive() && ImGui::GetCurrentContext()->HoveredIdTimer > 0.8f)
        ImGui::SetTooltip("Drag to tilt the stack, double-click for %s", vs.flat ? "3D + 2D" : "2D only");

    frameBg(dl, a, b);
    dl->PushClipRect(a, b, true);

    const WavetableSet* wt = overrideTable ? overrideTable : ui.s.wavetables[osc].load(std::memory_order_acquire);
    if (!wt) { dl->PopClipRect(); ImGui::PopID(); return; }
    WtCache& c = gCache[vi];
    if (c.src != wt || overrideTable) buildCache(c, wt);

    const bool live = ui.s.telemetry.displayVoiceActive.load(std::memory_order_relaxed);
    float pos;
    if (overridePos >= 0.0f) pos = overridePos;
    else pos = live ? ui.s.telemetry.oscPos[osc].load(std::memory_order_relaxed)
                    : ui.s.params.norm(oscParam(osc, P::OSCA_POS));
    pos = std::clamp(pos, 0.0f, 1.0f);

    // warp as currently heard (modulated when a voice is playing)
    int warp = (int)ui.s.params.value(oscParam(osc, P::OSCA_WARP));
    P amtP = oscParam(osc, P::OSCA_WARPAMT);
    float warpAmt = live ? paramInfo(amtP).toValue(ui.s.telemetry.modNorm[(int)amtP].load(std::memory_order_relaxed))
                         : ui.s.params.value(amtP);

    const float W = size.x;
    const float stripH = vs.flat ? size.y - 22.0f : std::floor(size.y * 0.22f);
    const float stackH = vs.flat ? 0.0f : size.y - stripH - 22.0f;
    const ImU32 green = theme::accent;

    // ---- header labels ----
    ImGui::PushFont(theme::fontSmall);
    dl->AddText(ImVec2(a.x + 8, a.y + 4), theme::textDim, wt->name.c_str());
    char buf[64];
    std::snprintf(buf, sizeof buf, "frame %.1f / %d", pos * (WT_FRAMES - 1) + 1.0f, WT_FRAMES);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(b.x - ts.x - 8, a.y + 4), green, buf);
    ImGui::PopFont();

    // ---- 3D stack ----
    if (!vs.flat) {
        const float top = a.y + 20.0f, H = stackH;
        const float bx0 = -1.08f, bx1 = 1.2f, by0 = -1.2f, by1 = 1.2f;   // box extents

        // Model space: x along the waveform (turned so the front corner faces the
        // viewer), z into the depth, y = amplitude. Scale and offset are fitted so
        // the whole box always fills the area.
        const float amp = 0.21f;
        const ImVec2 X(std::cos(vs.turn), std::sin(vs.turn)), Z(vs.zx, -vs.zy);
        auto model = [&](float x, float z, float y) { return ImVec2(x * X.x + z * Z.x, x * X.y + z * Z.y - y * amp); };
        float mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (float x : { bx0, bx1 }) for (float z : { 0.0f, 1.0f }) for (float y : { by0, by1 }) {
            ImVec2 m = model(x, z, y);
            mnx = std::min(mnx, m.x); mxx = std::max(mxx, m.x);
            mny = std::min(mny, m.y); mxy = std::max(mxy, m.y);
        }
        const float availW = W - 24.0f, availH = H - 6.0f;
        const float scale = std::min(availW / (mxx - mnx), availH / (mxy - mny));
        const ImVec2 origin(a.x + W * 0.5f - (mnx + mxx) * 0.5f * scale, top + H * 0.5f - (mny + mxy) * 0.5f * scale);
        auto project = [&](float x, float z, float y) { ImVec2 m = model(x, z, y); return ImVec2(origin.x + m.x * scale, origin.y + m.y * scale); };

        ImVec2 pts[PTS];
        auto framePts = [&](const float* data, float z) {
            for (int i = 0; i < PTS; ++i) pts[i] = project((float)i / (PTS - 1) * 2.0f - 1.0f, z, data[i]);
        };

        // grey box around the stack: back wall and floor filled, edges outlined
        const ImU32 boxFill = theme::rgba(24, 24, 27, 128), boxEdge = theme::rgba(70, 70, 76, 128);
        dl->AddQuadFilled(project(bx0, 1.0f, by0), project(bx1, 1.0f, by0), project(bx1, 1.0f, by1), project(bx0, 1.0f, by1), boxFill);
        dl->AddQuadFilled(project(bx0, 0.0f, by0), project(bx1, 0.0f, by0), project(bx1, 1.0f, by0), project(bx0, 1.0f, by0), theme::rgba(19, 19, 21, 128));
        auto edge = [&](float x0, float z0, float y0, float x1, float z1, float y1) {
            dl->AddLine(project(x0, z0, y0), project(x1, z1, y1), boxEdge, 1.0f);
        };
        // back edges (drawn under the frames)
        edge(bx0, 1, by0, bx1, 1, by0); edge(bx0, 1, by1, bx1, 1, by1);
        edge(bx0, 1, by0, bx0, 1, by1); edge(bx1, 1, by0, bx1, 1, by1);
        edge(bx0, 0, by0, bx0, 1, by0); edge(bx1, 0, by0, bx1, 1, by0);

        // depth rail on the right, with the position marker
        ImVec2 r0 = project(1.12f, 0.0f, 0.0f), r1 = project(1.12f, 1.0f, 0.0f);
        dl->AddLine(r0, r1, theme::withAlpha(green, 0.25f), 1.0f);
        for (int f = 0; f < WT_FRAMES; ++f) {
            ImVec2 t = project(1.12f, (float)f / (WT_FRAMES - 1), 0.0f);
            dl->AddLine(ImVec2(t.x - 3, t.y), ImVec2(t.x + 3, t.y), theme::withAlpha(green, 0.35f));
        }

        // frames, back to front, fading with distance from the playing position
        for (int f = WT_FRAMES - 1; f >= 0; --f) {
            float z = (float)f / (WT_FRAMES - 1);
            float near = std::max(0.0f, 1.0f - std::fabs(z - pos) * 2.5f);
            float alpha = 0.16f + 0.5f * near;
            framePts(c.frames[f], z);
            dl->AddPolyline(pts, PTS, theme::withAlpha(green, alpha), 0, 1.3f);
        }

        // playing position: interpolated frame, filled, on top
        float cur[PTS];
        {
            float fp = pos * (WT_FRAMES - 1);
            int f0 = std::min((int)fp, WT_FRAMES - 1), f1 = std::min(f0 + 1, WT_FRAMES - 1);
            float t = fp - f0;
            for (int i = 0; i < PTS; ++i) cur[i] = c.frames[f0][i] + (c.frames[f1][i] - c.frames[f0][i]) * t;
        }
        framePts(cur, pos);
        {
            NoAAFill noaa(dl);
            for (int i = 1; i < PTS; ++i) {
                ImVec2 z0 = project((float)(i - 1) / (PTS - 1) * 2.0f - 1.0f, pos, 0.0f);
                ImVec2 z1 = project((float)i / (PTS - 1) * 2.0f - 1.0f, pos, 0.0f);
                dl->AddQuadFilled(pts[i - 1], pts[i], z1, z0, theme::withAlpha(green, 0.12f));
            }
        }
        dl->AddPolyline(pts, PTS, theme::withAlpha(green, 0.25f), 0, 7.0f);
        dl->AddPolyline(pts, PTS, green, 0, 2.4f);
        ImVec2 m = project(1.12f, pos, 0.0f);
        dl->AddCircleFilled(m, 4.5f, green);

        // front edges of the box (drawn over the frames)
        edge(bx0, 0, by0, bx1, 0, by0); edge(bx0, 0, by1, bx1, 0, by1);
        edge(bx0, 0, by0, bx0, 0, by1); edge(bx1, 0, by0, bx1, 0, by1);
        edge(bx0, 0, by1, bx0, 1, by1); edge(bx1, 0, by1, bx1, 1, by1);
    }

    // ---- 2D strip: what the oscillator plays, warp included ----
    {
        ImVec2 sa(a.x + 6, b.y - stripH - 6), sb(b.x - 6, b.y - 6);
        dl->AddRectFilled(sa, sb, theme::rgba(9, 9, 10), 3.0f);
        dl->AddRect(sa, sb, theme::withAlpha(green, 0.15f), 3.0f);
        float mid = (sa.y + sb.y) * 0.5f, amp = (sb.y - sa.y) * 0.42f;
        dl->AddLine(ImVec2(sa.x, mid), ImVec2(sb.x, mid), theme::withAlpha(green, 0.12f));
        ImVec2 p2[PTS2];
        const int level = 2;
        for (int i = 0; i < PTS2; ++i) {
            float ph = (float)i / PTS2;
            float v = readTable(wt, level, pos, (warp == 0 || warp == 5) ? ph : warpPhaseUi(ph, warp, warpAmt));
            if (warp == 5) v = std::sin(v * (1.0f + warpAmt * 5.0f) * dsp::PI * 0.5f);
            p2[i] = ImVec2(sa.x + 4 + (sb.x - sa.x - 8) * i / (PTS2 - 1), mid - std::clamp(v, -1.1f, 1.1f) * amp);
        }
        {
            NoAAFill noaa(dl);
            for (int i = 1; i < PTS2; ++i)
                dl->AddQuadFilled(p2[i - 1], p2[i], ImVec2(p2[i].x, mid), ImVec2(p2[i - 1].x, mid), theme::withAlpha(green, 0.10f));
        }
        dl->AddPolyline(p2, PTS2, green, 0, 1.8f);
        ImGui::PushFont(theme::fontSmall);
        char lab[48];
        if (warp == 0) std::snprintf(lab, sizeof lab, "output");
        else std::snprintf(lab, sizeof lab, "output  |  warp %s %.0f%%", kWarpModes[std::clamp(warp, 0, 5)], warpAmt * 100.0f);
        ImVec2 ls = ImGui::CalcTextSize(lab);
        dl->AddText(ImVec2(sb.x - ls.x - 6, sa.y + 3), theme::withAlpha(green, 0.6f), lab);
        ImGui::PopFont();
    }

    dl->PopClipRect();
    ImGui::PopID();
}

// ---------------------------------------------------------------------
// Envelope editor
// ---------------------------------------------------------------------
static float envShape(float t, float curve)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return std::pow(t, std::pow(2.0f, std::clamp(curve, -1.0f, 1.0f) * 4.0f));   // matches Envelope::shapeCurve
}

void EnvelopeEditor(UiState& ui, int env, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushID(env + 32000);
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##env", size);
    bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    frameBg(dl, a, b);

    auto ep = [&](P p) { return envParam(env, p); };
    auto val = [&](P p) { return ui.s.params.value(ep(p)); };
    float A = val(P::ENV1_ATK), Hd = val(P::ENV1_HOLD), D = val(P::ENV1_DEC), S = val(P::ENV1_SUS), R = val(P::ENV1_REL);
    float ac = val(P::ENV1_ACRV), dc = val(P::ENV1_DCRV), rc = val(P::ENV1_RCRV);

    const float pad = 10.0f;
    const float left = a.x + pad, right = b.x - pad, top = a.y + pad + 4, bottom = b.y - pad - 10;
    float sustainSeg = std::max(0.25f * (A + Hd + D + R), 0.15f);
    float total = std::max(A + Hd + D + sustainSeg + R, 0.3f);

    ImGuiStorage* st = ImGui::GetStateStorage();
    ImGuiID idH = ImGui::GetID("handle"), idPps = ImGui::GetID("pps");
    float pps = (right - left) / total;
    if (active) pps = st->GetFloat(idPps, pps);   // frozen scale while dragging

    float xA = left + A * pps, xH = xA + Hd * pps, xD = xH + D * pps, xS = xD + sustainSeg * pps, xR = xS + R * pps;
    auto yOf = [&](float lv) { return bottom - lv * (bottom - top); };

    // grid
    for (int g = 1; g < 4; ++g) dl->AddLine(ImVec2(left, top + (bottom - top) * g / 4), ImVec2(right, top + (bottom - top) * g / 4), theme::rgba(26, 26, 27));
    dl->AddLine(ImVec2(left, bottom), ImVec2(right, bottom), theme::rgba(40, 40, 41));

    // curve points
    std::vector<ImVec2> pts;
    auto seg = [&](float x0, float x1, float l0, float l1, float curve) {
        int n = std::max(2, (int)((x1 - x0) / 3.0f));
        for (int i = 0; i <= n; ++i) {
            float t = (float)i / n;
            pts.push_back(ImVec2(x0 + (x1 - x0) * t, yOf(l0 + (l1 - l0) * envShape(t, curve))));
        }
    };
    seg(left, xA, 0.0f, 1.0f, ac);
    pts.push_back(ImVec2(xH, yOf(1.0f)));
    seg(xH, xD, 1.0f, S, dc);
    pts.push_back(ImVec2(xS, yOf(S)));
    seg(xS, xR, S, 0.0f, rc);

    { NoAAFill noaa(dl);
    for (size_t i = 1; i < pts.size(); ++i)
        dl->AddQuadFilled(pts[i - 1], pts[i], ImVec2(pts[i].x, bottom), ImVec2(pts[i - 1].x, bottom), theme::withAlpha(theme::accent, 0.10f)); }
    dl->AddPolyline(pts.data(), (int)pts.size(), theme::accent, 0, 2.0f);

    // handles: 0 attack, 1 hold, 2 decay/sustain, 3 release, 4/5/6 curves
    struct Hd_ { ImVec2 p; int kind; };
    Hd_ handles[7] = {
        { ImVec2(xA, yOf(1.0f)), 0 }, { ImVec2(xH, yOf(1.0f)), 1 }, { ImVec2(xD, yOf(S)), 2 }, { ImVec2(xR, yOf(0.0f)), 3 },
        { ImVec2((left + xA) * 0.5f, yOf(envShape(0.5f, ac))), 4 },
        { ImVec2((xH + xD) * 0.5f, yOf(1.0f + (S - 1.0f) * envShape(0.5f, dc))), 5 },
        { ImVec2((xS + xR) * 0.5f, yOf(S * (1.0f - envShape(0.5f, rc)))), 6 },
    };

    if (ImGui::IsItemActivated()) {
        int best = -1;
        float bestD = 14.0f * 14.0f;
        for (auto& h : handles) {
            float dx = io.MousePos.x - h.p.x, dy = io.MousePos.y - h.p.y, d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = h.kind; }
        }
        st->SetInt(idH, best);
        st->SetFloat(idPps, pps);
    }
    int hot = active ? st->GetInt(idH, -1) : -1;
    if (hot >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        float mx = io.MousePos.x;
        auto setT = [&](P p, float sec) { ui.s.params.setValue(ep(p), std::clamp(sec, 0.0f, paramInfo(ep(p)).max)); };
        switch (hot) {
            case 0: setT(P::ENV1_ATK, (mx - left) / pps); break;
            case 1: setT(P::ENV1_HOLD, (mx - xA) / pps); break;
            case 2: setT(P::ENV1_DEC, (mx - xH) / pps);
                    ui.s.params.setValue(ep(P::ENV1_SUS), std::clamp((bottom - io.MousePos.y) / (bottom - top), 0.0f, 1.0f)); break;
            case 3: setT(P::ENV1_REL, (mx - xS) / pps); break;
            case 4: ui.s.params.setValue(ep(P::ENV1_ACRV), std::clamp(ac + io.MouseDelta.y * 0.012f, -1.0f, 1.0f)); break;
            case 5: ui.s.params.setValue(ep(P::ENV1_DCRV), std::clamp(dc - io.MouseDelta.y * 0.012f * (S < 1 ? 1 : -1), -1.0f, 1.0f)); break;
            case 6: ui.s.params.setValue(ep(P::ENV1_RCRV), std::clamp(rc - io.MouseDelta.y * 0.012f, -1.0f, 1.0f)); break;
        }
        ui.presetDirty = true;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        for (P p : { P::ENV1_ACRV, P::ENV1_DCRV, P::ENV1_RCRV })
            ui.s.params.setValue(ep(p), paramInfo(ep(p)).def);
    }

    for (auto& h : handles) {
        bool isCurve = h.kind >= 4;
        bool isHot = hot == h.kind;
        float hdx = io.MousePos.x - h.p.x, hdy = io.MousePos.y - h.p.y;
        bool near = hovered && hdx * hdx + hdy * hdy < 14.0f * 14.0f;
        if (isCurve) dl->AddCircle(h.p, isHot || near ? 5.0f : 3.5f, theme::withAlpha(theme::accent, isHot || near ? 1.0f : 0.5f), 12, 1.5f);
        else {
            dl->AddCircleFilled(h.p, isHot || near ? 6.0f : 4.5f, theme::rgba(14, 14, 15));
            dl->AddCircle(h.p, isHot || near ? 6.0f : 4.5f, theme::accent, 16, 2.0f);
        }
    }

    // playhead from the newest voice
    if (ui.s.telemetry.displayVoiceActive.load(std::memory_order_relaxed)) {
        int stage = ui.s.telemetry.envStage[env].load(std::memory_order_relaxed);
        float t = ui.s.telemetry.envStageTime[env].load(std::memory_order_relaxed);
        float level = ui.s.telemetry.srcValue[(int)ModSrc::Env1 + env].load(std::memory_order_relaxed);
        float x = -1.0f;
        switch (stage) {
            case 1: x = left + std::min(t, A) * pps; break;
            case 2: x = xA + std::min(t, Hd) * pps; break;
            case 3: x = xH + std::min(t, D) * pps; break;
            case 4: x = xD + frac01((float)ImGui::GetTime() * 0.5f) * (xS - xD); break;
            case 5: x = xS + std::min(t, R) * pps; break;
        }
        if (x >= 0.0f) {
            ImVec2 p(x, yOf(level));
            dl->AddLine(ImVec2(x, top), ImVec2(x, bottom), theme::withAlpha(theme::accent, 0.15f));
            dl->AddCircleFilled(p, 7.0f, theme::withAlpha(theme::accent, 0.25f));
            dl->AddCircleFilled(p, 3.5f, IM_COL32_WHITE);
        }
    }

    // readout
    ImGui::PushFont(theme::fontSmall);
    char buf[96];
    auto fmt = [](float s) { static char b[4][16]; static int k = 0; k = (k + 1) & 3;
        if (s < 1.0f) std::snprintf(b[k], 16, "%.0fms", s * 1000.0f); else std::snprintf(b[k], 16, "%.2fs", s); return b[k]; };
    std::snprintf(buf, sizeof buf, "A %s   H %s   D %s   S %.0f%%   R %s", fmt(A), fmt(Hd), fmt(D), S * 100.0f, fmt(R));
    dl->AddText(ImVec2(left, b.y - 16), theme::textDim, buf);
    ImGui::PopFont();
    ImGui::PopID();
}

// ---------------------------------------------------------------------
// LFO
// ---------------------------------------------------------------------
void LfoShapeIcon(UiState& ui, int lfo, ImDrawList* dl, int shape, ImVec2 a, ImVec2 b, ImU32 col)
{
    ImVec2 pts[24];
    for (int i = 0; i < 24; ++i) {
        float p = i / 23.0f;
        float v = shape == (int)LfoShape::Custom ? evalLfoCurve(ui.wt.lfoCurve(lfo), p) : lfoShapePreview((LfoShape)shape, p);
        pts[i] = ImVec2(a.x + (b.x - a.x) * p, (a.y + b.y) * 0.5f - v * (b.y - a.y) * 0.45f);
    }
    dl->AddPolyline(pts, 24, col, 0, 1.5f);
}

void LfoGraph(UiState& ui, int lfo, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushID(lfo + 34000);
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    auto shape = (LfoShape)(int)ui.s.params.value(lfoParam(lfo, P::LFO1_SHAPE));
    const bool custom = shape == LfoShape::Custom;
    ImGui::InvisibleButton("##lfo", size);
    const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    frameBg(dl, a, b);

    const float pad = 10.0f, mid = (a.y + b.y) * 0.5f, amp = (size.y - pad * 2) * 0.45f;
    const float x0 = a.x + pad, w = size.x - pad * 2;
    auto toScreen = [&](float x, float y) { return ImVec2(x0 + x * w, mid - y * amp); };
    dl->AddLine(ImVec2(x0, mid), ImVec2(x0 + w, mid), theme::rgba(30, 30, 33));
    for (int q = 1; q < 4; ++q) {
        float x = x0 + w * q / 4.0f;
        dl->AddLine(ImVec2(x, a.y + pad), ImVec2(x, b.y - pad), theme::rgba(22, 22, 25));
    }

    // ---- custom shape editing ----
    std::vector<LfoPoint> pts = ui.wt.lfoCurve(lfo);
    int hoverPt = -1, hoverSeg = -1;
    if (custom) {
        float bestD = 10.0f * 10.0f;
        for (int i = 0; i < (int)pts.size(); ++i) {
            ImVec2 p = toScreen(pts[i].x, pts[i].y);
            float d = (p.x - io.MousePos.x) * (p.x - io.MousePos.x) + (p.y - io.MousePos.y) * (p.y - io.MousePos.y);
            if (d < bestD) { bestD = d; hoverPt = i; }
        }
        if (hoverPt < 0) {
            for (int i = 0; i + 1 < (int)pts.size(); ++i) {
                float mx = 0.5f * (pts[i].x + pts[i + 1].x);
                ImVec2 p = toScreen(mx, evalLfoCurve(pts, mx));
                float d = (p.x - io.MousePos.x) * (p.x - io.MousePos.x) + (p.y - io.MousePos.y) * (p.y - io.MousePos.y);
                if (d < bestD) { bestD = d; hoverSeg = i; }
            }
        }

        ImGuiStorage* st = ImGui::GetStateStorage();
        ImGuiID idPt = ImGui::GetID("pt"), idSeg = ImGui::GetID("seg");
        bool changed = false;
        float mx = std::clamp((io.MousePos.x - x0) / w, 0.0f, 1.0f);
        float my = std::clamp((mid - io.MousePos.y) / amp, -1.0f, 1.0f);

        if (ImGui::IsItemActivated()) {
            if (hoverPt >= 0)       { st->SetInt(idPt, hoverPt); st->SetInt(idSeg, -1); }
            else if (hoverSeg >= 0) { st->SetInt(idPt, -1); st->SetInt(idSeg, hoverSeg); }
            else {                  // add a point and start dragging it
                LfoPoint np{ mx, my, 0.0f };
                int at = 0;
                while (at < (int)pts.size() && pts[at].x < mx) ++at;
                if (at == 0) at = 1;
                if (at >= (int)pts.size()) at = (int)pts.size() - 1;
                pts.insert(pts.begin() + at, np);
                st->SetInt(idPt, at); st->SetInt(idSeg, -1);
                changed = true;
            }
        }
        if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            int pi = st->GetInt(idPt, -1), si = st->GetInt(idSeg, -1);
            if (pi >= 0 && pi < (int)pts.size()) {
                bool end = pi == 0 || pi == (int)pts.size() - 1;
                if (!end) pts[pi].x = std::clamp(mx, pts[pi - 1].x + 0.001f, pts[pi + 1].x - 0.001f);
                pts[pi].y = my;
                changed = true;
            } else if (si >= 0 && si + 1 < (int)pts.size()) {
                float dir = pts[si + 1].y >= pts[si].y ? 1.0f : -1.0f;
                pts[si].curve = std::clamp(pts[si].curve + io.MouseDelta.y * 0.012f * dir, -1.0f, 1.0f);
                changed = true;
            }
        }
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (hoverPt > 0 && hoverPt < (int)pts.size() - 1) { pts.erase(pts.begin() + hoverPt); changed = true; }
            else if (hoverSeg >= 0) { pts[hoverSeg].curve = 0.0f; changed = true; }
        }
        if (changed) { ui.wt.setLfoCurve(lfo, pts); pts = ui.wt.lfoCurve(lfo); ui.presetDirty = true; }
        if (hovered && !active && ImGui::GetCurrentContext()->HoveredIdTimer > 0.8f)
            ImGui::SetTooltip("Click: add point   Drag: move point / bend segment\nRight-click: delete point / straighten segment");
    }

    // ---- curve ----
    const int N = 200;
    ImVec2 line[N];
    for (int i = 0; i < N; ++i) {
        float p = (float)i / (N - 1);
        float y = custom ? evalLfoCurve(pts, p) : lfoShapePreview(shape, p);
        line[i] = toScreen(p, y);
    }
    {
        NoAAFill noaa(dl);
        for (int i = 1; i < N; ++i)
            dl->AddQuadFilled(line[i - 1], line[i], ImVec2(line[i].x, mid), ImVec2(line[i - 1].x, mid), theme::withAlpha(theme::mod, 0.08f));
    }
    dl->AddPolyline(line, N, theme::mod, 0, 2.0f);

    if (custom) {
        for (int i = 0; i + 1 < (int)pts.size(); ++i) {
            float mx = 0.5f * (pts[i].x + pts[i + 1].x);
            ImVec2 p = toScreen(mx, evalLfoCurve(pts, mx));
            dl->AddCircle(p, i == hoverSeg ? 5.0f : 3.5f, theme::withAlpha(theme::mod, i == hoverSeg ? 1.0f : 0.45f), 12, 1.5f);
        }
        for (int i = 0; i < (int)pts.size(); ++i) {
            ImVec2 p = toScreen(pts[i].x, pts[i].y);
            float r = i == hoverPt ? 6.0f : 4.5f;
            dl->AddCircleFilled(p, r, theme::rgba(11, 11, 12));
            dl->AddCircle(p, r, theme::mod, 16, 2.0f);
        }
    }

    // ---- live phase ----
    float ph = frac01(ui.s.telemetry.lfoPhase[lfo].load(std::memory_order_relaxed));
    float v = ui.s.telemetry.srcValue[(int)ModSrc::Lfo1 + lfo].load(std::memory_order_relaxed);
    float x = x0 + w * ph;
    dl->AddLine(ImVec2(x, a.y + pad), ImVec2(x, b.y - pad), theme::withAlpha(theme::mod, 0.3f));
    ImVec2 dot(x, mid - std::clamp(v, -1.0f, 1.0f) * amp);
    dl->AddCircleFilled(dot, 7.0f, theme::withAlpha(theme::mod, 0.25f));
    dl->AddCircleFilled(dot, 3.5f, theme::accent);
    ImGui::PopID();
}

// ---------------------------------------------------------------------
// Filter response
// ---------------------------------------------------------------------
static float filterMagDb(int type, float f, float cutoff, float res)
{
    bool steep = type == 1 || type == 3 || type == 5;
    int mode = type < 2 ? 0 : type < 4 ? 1 : type < 6 ? 2 : type == 6 ? 3 : 4;
    float m;
    if (steep) {
        float k2 = 0.7654f + (0.05f - 0.7654f) * res;
        m = dsp::svfMagnitude(mode, f, cutoff, 1.8478f) * dsp::svfMagnitude(mode, f, cutoff, k2);
        if (mode == 1) m *= 1.8478f * k2;
    } else {
        float k = 2.0f + (0.05f - 2.0f) * res;
        m = dsp::svfMagnitude(mode, f, cutoff, k);
        if (mode == 1) m *= k;
    }
    return 20.0f * std::log10(std::max(m, 1e-6f));
}

void FilterResponse(UiState& ui, int filter, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::PushID(filter + 33000);
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton("##flt", size);
    bool active = ImGui::IsItemActive();
    frameBg(dl, a, b);

    auto fp = [&](P p) { return fltParam(filter, p); };
    const float fMin = 20.0f, fMax = 20000.0f, dbTop = 24.0f, dbBot = -36.0f;
    auto xOf = [&](float f) { return a.x + size.x * std::log(f / fMin) / std::log(fMax / fMin); };
    auto yOf = [&](float db) { return a.y + size.y * (dbTop - std::clamp(db, dbBot, dbTop)) / (dbTop - dbBot); };

    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
        float t = std::clamp((io.MousePos.x - a.x) / size.x, 0.0f, 1.0f);
        ui.s.params.setValue(fp(P::FLT_CUTOFF), fMin * std::pow(fMax / fMin, t));
        ui.s.params.setValue(fp(P::FLT_RES), std::clamp(1.0f - (io.MousePos.y - a.y) / size.y, 0.0f, 1.0f));
        ui.presetDirty = true;
    }
    if (ImGui::IsItemHovered() && !active && ImGui::GetCurrentContext()->HoveredIdTimer > 0.8f)
        ImGui::SetTooltip("Drag: left/right = cutoff, up/down = resonance");

    for (float f : { 100.0f, 1000.0f, 10000.0f }) dl->AddLine(ImVec2(xOf(f), a.y), ImVec2(xOf(f), b.y), theme::rgba(30, 30, 33));
    for (float f : { 50.f, 200.f, 500.f, 2000.f, 5000.f }) dl->AddLine(ImVec2(xOf(f), a.y), ImVec2(xOf(f), b.y), theme::rgba(22, 22, 25));
    dl->AddLine(ImVec2(a.x, yOf(0.0f)), ImVec2(b.x, yOf(0.0f)), theme::rgba(40, 40, 44));

    auto draw = [&](int f, float cutoff, float res, ImU32 col, float thick, bool fill) {
        int type = (int)ui.s.params.value(fltParam(f, P::FLT_TYPE));
        bool on = ui.s.params.value(fltParam(f, P::FLT_ON)) > 0.5f;
        const int N = 140;
        ImVec2 pts[N];
        for (int i = 0; i < N; ++i) {
            float fr = fMin * std::pow(fMax / fMin, (float)i / (N - 1));
            pts[i] = ImVec2(xOf(fr), yOf(on ? filterMagDb(type, fr, cutoff, res) : 0.0f));
        }
        if (fill) {
            NoAAFill noaa(dl);
            for (int i = 1; i < N; ++i)
                dl->AddQuadFilled(pts[i - 1], pts[i], ImVec2(pts[i].x, b.y), ImVec2(pts[i - 1].x, b.y), theme::withAlpha(col, 0.08f));
        }
        dl->AddPolyline(pts, N, col, 0, thick);
    };

    // the other filter, dimmed, for context
    int other = 1 - filter;
    if (ui.s.params.value(fltParam(other, P::FLT_ON)) > 0.5f)
        draw(other, ui.s.params.value(fltParam(other, P::FLT_CUTOFF)), ui.s.params.value(fltParam(other, P::FLT_RES)),
             theme::withAlpha(theme::text, 0.25f), 1.2f, false);

    float cutoff = ui.s.params.value(fp(P::FLT_CUTOFF)), res = ui.s.params.value(fp(P::FLT_RES));
    draw(filter, cutoff, res, theme::accent, 2.0f, true);

    bool live = ui.s.telemetry.displayVoiceActive.load(std::memory_order_relaxed);
    float lc = paramInfo(fp(P::FLT_CUTOFF)).toValue(ui.s.telemetry.modNorm[(int)fp(P::FLT_CUTOFF)].load(std::memory_order_relaxed));
    float lr = paramInfo(fp(P::FLT_RES)).toValue(ui.s.telemetry.modNorm[(int)fp(P::FLT_RES)].load(std::memory_order_relaxed));
    bool on = ui.s.params.value(fp(P::FLT_ON)) > 0.5f;
    if (live && on && (std::fabs(lc - cutoff) > 1.0f || std::fabs(lr - res) > 0.005f))
        draw(filter, lc, lr, theme::mod, 1.6f, false);

    int type = (int)ui.s.params.value(fp(P::FLT_TYPE));
    ImVec2 handle(xOf(cutoff), yOf(on ? filterMagDb(type, cutoff, cutoff, res) : 0.0f));
    dl->AddCircleFilled(handle, 5.0f, theme::rgba(11, 11, 12));
    dl->AddCircle(handle, 5.0f, theme::accent, 16, 2.0f);

    ImGui::PushFont(theme::fontSmall);
    for (auto [f, l] : { std::pair{ 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } })
        dl->AddText(ImVec2(xOf(f) + 3, b.y - 15), theme::textFaint, l);
    ImGui::PopFont();
    ImGui::PopID();
}

// ---------------------------------------------------------------------
// Scope & spectrum
// ---------------------------------------------------------------------
static void copyRecent(const Telemetry& t, float* out, int n)
{
    uint32_t w = t.scopeWrite.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) out[i] = t.scope[(w - n + i) & (Telemetry::SCOPE_SIZE - 1)];
}

void Oscilloscope(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    frameBg(dl, a, b);
    float mid = (a.y + b.y) * 0.5f;
    dl->AddLine(ImVec2(a.x, mid), ImVec2(b.x, mid), theme::rgba(30, 30, 31));

    static float buf[2048];
    copyRecent(ui.s.telemetry, buf, 2048);
    int start = 0;
    for (int i = 1; i < 1024; ++i)
        if (buf[i - 1] <= 0.0f && buf[i] > 0.0f) { start = i; break; }   // rising-edge trigger

    const int N = 512;
    ImVec2 pts[N];
    for (int i = 0; i < N; ++i)
        pts[i] = ImVec2(a.x + size.x * i / (N - 1), mid - std::clamp(buf[start + i * 2], -1.0f, 1.0f) * size.y * 0.45f);
    dl->PushClipRect(a, b, true);
    dl->AddPolyline(pts, N, theme::withAlpha(theme::accent, 0.2f), 0, 4.0f);
    dl->AddPolyline(pts, N, theme::accent, 0, 1.4f);
    dl->PopClipRect();
}

void Spectrum(UiState& ui, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::Dummy(size);
    frameBg(dl, a, b);

    constexpr int N = 2048;
    static std::vector<float> re(N), im(N), smooth(N / 2, -120.0f);
    static float win[N];
    static bool init = false;
    if (!init) { for (int i = 0; i < N; ++i) win[i] = 0.5f - 0.5f * std::cos(2.0f * 3.14159265f * i / (N - 1)); init = true; }

    copyRecent(ui.s.telemetry, re.data(), N);
    for (int i = 0; i < N; ++i) { re[i] *= win[i]; im[i] = 0.0f; }
    fftComplex(re, im, false);
    for (int i = 0; i < N / 2; ++i) {
        float mag = std::sqrt(re[i] * re[i] + im[i] * im[i]) * (4.0f / N);
        float db = 20.0f * std::log10(std::max(mag, 1e-7f));
        smooth[i] = db > smooth[i] ? db : smooth[i] - 1.2f;
    }

    float sr = ui.audio.sampleRate() ? (float)ui.audio.sampleRate() : 48000.0f;
    const float fMin = 30.0f, fMax = 20000.0f, dbTop = 0.0f, dbBot = -90.0f;
    const int P = 120;
    ImVec2 pts[P];
    for (int i = 0; i < P; ++i) {
        float f0 = fMin * std::pow(fMax / fMin, (float)i / P), f1 = fMin * std::pow(fMax / fMin, (float)(i + 1) / P);
        int b0 = std::clamp((int)(f0 / sr * N), 1, N / 2 - 1), b1 = std::clamp((int)(f1 / sr * N), b0 + 1, N / 2);
        float m = -120.0f;
        for (int k = b0; k < b1; ++k) m = std::max(m, smooth[k]);
        float t = std::clamp((m - dbBot) / (dbTop - dbBot), 0.0f, 1.0f);
        pts[i] = ImVec2(a.x + size.x * i / (P - 1), b.y - t * (size.y - 4));
    }
    { NoAAFill noaa(dl);
    for (int i = 1; i < P; ++i)
        dl->AddQuadFilled(pts[i - 1], pts[i], ImVec2(pts[i].x, b.y), ImVec2(pts[i - 1].x, b.y), theme::withAlpha(theme::mod, 0.18f)); }
    dl->AddPolyline(pts, P, theme::withAlpha(theme::mod, 0.9f), 0, 1.4f);
}

// ---------------------------------------------------------------------
// Piano
// ---------------------------------------------------------------------
static bool isBlack(int n) { int k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

void Piano(UiState& ui, ImVec2 size, int lo, int hi)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##piano", size);
    bool active = ImGui::IsItemActive();

    int whites = 0;
    for (int n = lo; n <= hi; ++n) if (!isBlack(n)) ++whites;
    float ww = size.x / whites, bw = ww * 0.62f, bh = size.y * 0.62f;

    auto whiteX = [&](int note) { int c = 0; for (int n = lo; n < note; ++n) if (!isBlack(n)) ++c; return a.x + c * ww; };
    auto noteAt = [&](ImVec2 m) {
        if (m.x < a.x || m.x >= a.x + size.x || m.y < a.y || m.y >= a.y + size.y) return -1;
        if (m.y < a.y + bh)
            for (int n = lo; n <= hi; ++n)
                if (isBlack(n)) { float x = whiteX(n) - bw * 0.5f; if (m.x >= x && m.x < x + bw) return n; }
        int idx = (int)((m.x - a.x) / ww), c = 0;
        for (int n = lo; n <= hi; ++n) if (!isBlack(n)) { if (c == idx) return n; ++c; }
        return -1;
    };

    // mouse playing (glissando while dragging)
    static int mouseNote = -1;
    if (active) {
        int n = noteAt(io.MousePos);
        if (n != mouseNote) {
            if (mouseNote >= 0) ui.s.uiEvents.push({ EventType::NoteOff, (uint8_t)mouseNote, 0 });
            if (n >= 0) {
                float keyH = isBlack(n) ? bh : size.y;
                int vel = std::clamp((int)(35 + 92 * (io.MousePos.y - a.y) / keyH), 1, 127);
                ui.s.uiEvents.push({ EventType::NoteOn, (uint8_t)n, (uint8_t)vel });
            }
            mouseNote = n;
        }
    } else if (mouseNote >= 0) {
        ui.s.uiEvents.push({ EventType::NoteOff, (uint8_t)mouseNote, 0 });
        mouseNote = -1;
    }

    const Telemetry& t = ui.s.telemetry;
    int kbLo = (ui.keyOctave + 1) * 12;
    static const char* kKeys = "AWSEDFTGYHUJKOLP";
    ImGui::PushFont(theme::fontSmall);

    for (int n = lo; n <= hi; ++n) {
        if (isBlack(n)) continue;
        float x = whiteX(n);
        bool held = t.noteHeld(n) || n == mouseNote;
        ImVec2 p0(x + 1, a.y), p1(x + ww - 1, a.y + size.y);
        dl->AddRectFilledMultiColor(p0, p1, held ? theme::accent : theme::keyWhite, held ? theme::accent : theme::keyWhite,
                                    held ? theme::rgba(0, 170, 45) : theme::rgba(176, 180, 176), held ? theme::rgba(0, 170, 45) : theme::rgba(176, 180, 176));
        if (n % 12 == 0) {
            char b[8]; std::snprintf(b, sizeof b, "C%d", n / 12 - 1);
            dl->AddText(ImVec2(x + 4, a.y + size.y - 16), theme::rgba(80, 80, 81), b);
        }
        int k = n - kbLo;
        if (k >= 0 && k < 16) {
            char c[2] = { kKeys[k], 0 };
            dl->AddText(ImVec2(x + ww * 0.5f - 3, a.y + size.y - 32), theme::rgba(0, 150, 40), c);
        }
    }
    for (int n = lo; n <= hi; ++n) {
        if (!isBlack(n)) continue;
        float x = whiteX(n) - bw * 0.5f;
        bool held = t.noteHeld(n) || n == mouseNote;
        dl->AddRectFilled(ImVec2(x, a.y), ImVec2(x + bw, a.y + bh), held ? theme::rgba(0, 210, 55) : theme::keyBlack, 2.0f);
        if (!held) dl->AddRectFilled(ImVec2(x + 2, a.y), ImVec2(x + bw - 2, a.y + bh * 0.9f), theme::rgba(33, 33, 34), 2.0f);
        int k = n - kbLo;
        if (k >= 0 && k < 16) {
            char c[2] = { kKeys[k], 0 };
            dl->AddText(ImVec2(x + bw * 0.5f - 3, a.y + bh - 18), theme::rgba(0, 150, 40), c);
        }
    }
    ImGui::PopFont();
    dl->AddLine(ImVec2(a.x, a.y), ImVec2(a.x + size.x, a.y), theme::accentDim, 2.0f);
}

// ---------------------------------------------------------------------
// Wheels
// ---------------------------------------------------------------------
static bool wheel(const char* id, ImVec2 size, float& v, bool bipolar, bool spring, ImU32 col)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + size.x, a.y + size.y);
    ImGui::InvisibleButton(id, size);
    bool changed = false;
    if (ImGui::IsItemActive()) {
        float t = std::clamp(1.0f - (ImGui::GetIO().MousePos.y - a.y) / size.y, 0.0f, 1.0f);
        v = bipolar ? t * 2.0f - 1.0f : t;
        changed = true;
    } else if (spring && ImGui::IsItemDeactivated()) {
        v = 0.0f;   // spring back only after the user lets go of the on-screen wheel
        changed = true;
    }
    dl->AddRectFilled(a, b, theme::rgba(16, 16, 17), 5.0f);
    dl->AddRect(a, b, theme::rgba(39, 39, 40), 5.0f);
    float t = bipolar ? (v + 1.0f) * 0.5f : v;
    float y = b.y - t * size.y;
    float y0 = bipolar ? (a.y + b.y) * 0.5f : b.y;
    dl->AddRectFilled(ImVec2(a.x + 3, std::min(y, y0)), ImVec2(b.x - 3, std::max(y, y0)), theme::withAlpha(col, 0.35f), 3.0f);
    dl->AddRectFilled(ImVec2(a.x + 2, y - 3), ImVec2(b.x - 2, y + 3), col, 2.0f);
    return changed;
}

void Wheels(UiState& ui, ImVec2 size)
{
    static float pb = 0.0f, mw = 0.0f;
    float wd = (size.x - 8) * 0.5f;

    // reflect hardware wheels when not dragging
    float hwMw = ui.s.telemetry.srcValue[(int)ModSrc::ModWheel].load(std::memory_order_relaxed);
    float hwPb = ui.s.telemetry.srcValue[(int)ModSrc::PitchBend].load(std::memory_order_relaxed);

    ImGui::BeginGroup();
    float shownPb = hwPb;
    if (wheel("##pb", ImVec2(wd, size.y - 16), shownPb, true, true, theme::accent)) {
        pb = shownPb;
        ui.s.uiEvents.push({ EventType::PitchBend, 0, 0, pb });
    }
    ImGui::PushFont(theme::fontSmall); ImGui::TextDisabled(" PB"); ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::SameLine(0, 8);
    ImGui::BeginGroup();
    float shownMw = hwMw;
    if (wheel("##mw", ImVec2(wd, size.y - 16), shownMw, false, false, theme::mod)) {
        mw = shownMw;
        ui.s.uiEvents.push({ EventType::ControlChange, 1, (uint8_t)std::lround(mw * 127.0f) });
    }
    ImGui::PushFont(theme::fontSmall); ImGui::TextDisabled(" MOD"); ImGui::PopFont();
    ImGui::EndGroup();
}

} // namespace views
