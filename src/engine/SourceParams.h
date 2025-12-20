#pragma once

// Per-source parameters (modulatable)
// These are NOT per-voice; they are global patch params.
struct SourceParams {
    // LEVEL
    float levelBase = 1.0f;
    float levelMod  = 1.0f;

    // PAN (-1 .. +1)
    float panBase   = 0.0f;
    float panMod    = 0.0f;

    // PITCH (semitones)
    float pitchBase = 0.0f;
    float pitchMod  = 0.0f;
};
