#pragma once

struct NoiseParams {
    float pitchBase = 0.0f;
    float pitchMod  = 0.0f;

    float levelBase = dbToLin(-24.0f);
    float levelMod  = 0.0f;

    float panBase   = 0.0f;
    float panMod    = 0.0f;
};
