#pragma once

enum class NoiseMode : uint8_t {
    WHITE = 0,
    AC_HUM = 1
};

struct NoiseParams {
    NoiseMode mode = NoiseMode::WHITE;   // 👈 NEW

    float pitchBase = 0.0f;
    float pitchMod  = 0.0f;

    float levelBase = dbToLin(-24.0f);
    float levelMod  = 0.0f;

    float panBase   = 0.0f;
    float panMod    = 0.0f;
};
