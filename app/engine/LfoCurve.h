#pragma once

#include <vector>

// User-drawn LFO shape: breakpoints with a curve per segment.
// The UI edits points and renders them into a table; the audio thread only
// reads the table (published with an atomic pointer, like wavetables).

struct LfoPoint {
    float x;       // 0..1 (first point is pinned to 0, last to 1)
    float y;       // -1..1
    float curve;   // bend of the segment that starts here, -1..1
};

constexpr int LFO_CURVE_SIZE = 256;

struct LfoCurveTable {
    float v[LFO_CURVE_SIZE + 1];   // last sample = value at x = 1
};

std::vector<LfoPoint> defaultLfoCurve();
float evalLfoCurve(const std::vector<LfoPoint>& pts, float x);
void  renderLfoCurve(const std::vector<LfoPoint>& pts, LfoCurveTable& out);
void  sanitizeLfoCurve(std::vector<LfoPoint>& pts);   // sort, pin ends, clamp

inline float lookupLfoCurve(const LfoCurveTable& t, float phase)
{
    float pos = phase * LFO_CURVE_SIZE;
    int i = (int)pos;
    if (i >= LFO_CURVE_SIZE) i = LFO_CURVE_SIZE - 1;
    float f = pos - (float)i;
    return t.v[i] + (t.v[i + 1] - t.v[i]) * f;
}
