#include "engine/Presets.h"

// basic-shapes wavetable frame positions
constexpr float WT_SINE   = 0.0f;
constexpr float WT_TRI    = 1.0f / 3.0f;
constexpr float WT_SAW    = 2.0f / 3.0f;
constexpr float WT_SQUARE = 1.0f;

static constexpr int A = 0, B = 1, SUB = 3;

static void toFilter(Patch& p, int src)
{
    p.sources[src].enabled = true;
    p.sources[src].dest = SourceDest::FILTER;
}

// Detuned saw + sine sub through a gentle low-pass, with some reverb.
static Patch warmSaw()
{
    Patch p;
    p.name = "Warm Saw";

    p.oscA.wtPosBase    = WT_SAW;
    p.oscA.levelBase    = dbToLin(-8.0f);
    p.oscA.unisonVoices = 3.0f;
    p.oscA.detune       = 12.0f;
    p.oscA.stereoSpread = 0.5f;
    toFilter(p, A);

    p.sub.mode         = OscMode::SINE;
    p.sub.semitoneBase = -12.0f;
    p.sub.levelBase    = dbToLin(-14.0f);
    toFilter(p, SUB);

    p.filter0.cutoffBase = 2500.0f;
    p.filter0.resBase    = 0.2f;
    p.filter0Route.bus1Send = 0.25f;

    p.attack = 0.01f; p.decay = 0.3f; p.sustain = 0.8f; p.release = 0.4f;
    return p;
}

// Square + saw an octave up, fast decay to silence, into the delay.
static Patch pluck()
{
    Patch p;
    p.name = "Pluck";

    p.oscA.wtPosBase = WT_SQUARE;
    p.oscA.levelBase = dbToLin(-9.0f);
    toFilter(p, A);

    p.oscB.wtPosBase    = WT_SAW;
    p.oscB.semitoneBase = 12.0f;
    p.oscB.levelBase    = dbToLin(-15.0f);
    toFilter(p, B);

    p.filter0.cutoffBase = 1800.0f;
    p.filter0.resBase    = 0.3f;
    p.filter0Route.bus1Send = 0.1f;
    p.filter0Route.bus2Send = 0.35f;

    p.attack = 0.002f; p.decay = 0.25f; p.sustain = 0.0f; p.release = 0.2f;
    return p;
}

// Wide detuned triangles plus a fifth, slow attack and long release.
static Patch softPad()
{
    Patch p;
    p.name = "Soft Pad";

    p.oscA.wtPosBase    = WT_TRI;
    p.oscA.levelBase    = dbToLin(-9.0f);
    p.oscA.unisonVoices = 5.0f;
    p.oscA.detune       = 20.0f;
    p.oscA.stereoSpread = 0.8f;
    toFilter(p, A);

    p.oscB.mode         = OscMode::SINE;
    p.oscB.semitoneBase = 7.0f;
    p.oscB.levelBase    = dbToLin(-16.0f);
    toFilter(p, B);

    p.filter0.cutoffBase = 1200.0f;
    p.filter0.resBase    = 0.1f;
    p.filter0Route.bus1Send = 0.5f;
    p.filter0Route.bus2Send = 0.15f;

    p.attack = 0.6f; p.decay = 1.0f; p.sustain = 0.8f; p.release = 1.5f;
    return p;
}

// Dark resonant saw with a strong sub, dry.
static Patch subBass()
{
    Patch p;
    p.name = "Sub Bass";

    p.oscA.wtPosBase    = WT_SAW;
    p.oscA.levelBase    = dbToLin(-9.0f);
    p.oscA.semitoneBase = -12.0f;
    toFilter(p, A);

    p.sub.mode         = OscMode::SINE;
    p.sub.semitoneBase = -24.0f;
    p.sub.levelBase    = dbToLin(-8.0f);
    toFilter(p, SUB);

    p.filter0.cutoffBase = 600.0f;
    p.filter0.resBase    = 0.4f;

    p.attack = 0.003f; p.decay = 0.2f; p.sustain = 0.6f; p.release = 0.12f;
    return p;
}

const Patch PRESETS[NUM_PRESETS] = {
    warmSaw(),
    pluck(),
    softPad(),
    subBass(),
};
