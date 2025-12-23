#pragma once

// ============================
// SOURCE → DESTINATION
// ============================

enum class SourceDest {
    NONE,     // signal terminates
    DIRECT,  // dry bypass
    FX,      // main / FX path
    FILTER   // goes into filter chain
};

// Per-source routing config (UI / preset driven)
struct SourceRouteConfig {
    SourceDest dest = SourceDest::NONE;

    // Only used when dest == FILTER
    // 0.0 = 100% Filter0
    // 1.0 = 100% Filter1
    float filterBlend = 0.0f;
};

// ============================
// FILTER → DESTINATION
// ============================

enum class FilterRoute {
    NONE,            // terminate signal
    TO_FX,
    TO_DIRECT,
    TO_OTHER_FILTER
};
