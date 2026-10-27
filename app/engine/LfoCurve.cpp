#include "engine/LfoCurve.h"

#include <algorithm>
#include <cmath>

std::vector<LfoPoint> defaultLfoCurve()
{
    return { { 0.0f, -1.0f, 0.5f }, { 0.25f, 1.0f, -0.5f }, { 1.0f, -1.0f, 0.0f } };
}

static float bend(float t, float curve)
{
    return std::pow(std::clamp(t, 0.0f, 1.0f), std::pow(2.0f, std::clamp(curve, -1.0f, 1.0f) * 3.0f));
}

float evalLfoCurve(const std::vector<LfoPoint>& pts, float x)
{
    if (pts.empty()) return 0.0f;
    if (pts.size() == 1 || x <= pts.front().x) return pts.front().y;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (x <= pts[i].x) {
            const LfoPoint& a = pts[i - 1];
            const LfoPoint& b = pts[i];
            float span = b.x - a.x;
            float t = span > 1e-6f ? (x - a.x) / span : 1.0f;
            return a.y + (b.y - a.y) * bend(t, a.curve);
        }
    }
    return pts.back().y;
}

void renderLfoCurve(const std::vector<LfoPoint>& pts, LfoCurveTable& out)
{
    for (int i = 0; i < LFO_CURVE_SIZE; ++i)
        out.v[i] = evalLfoCurve(pts, (float)i / LFO_CURVE_SIZE);
    out.v[LFO_CURVE_SIZE] = evalLfoCurve(pts, 1.0f);
}

void sanitizeLfoCurve(std::vector<LfoPoint>& pts)
{
    if (pts.size() < 2) pts = defaultLfoCurve();
    if (pts.size() > 32) pts.resize(32);
    std::sort(pts.begin(), pts.end(), [](const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    for (auto& p : pts) {
        p.x = std::clamp(p.x, 0.0f, 1.0f);
        p.y = std::clamp(p.y, -1.0f, 1.0f);
        p.curve = std::clamp(p.curve, -1.0f, 1.0f);
    }
    pts.front().x = 0.0f;
    pts.back().x = 1.0f;
}
