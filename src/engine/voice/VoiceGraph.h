#pragma once

enum class SourceDest {
    NONE,
    FILTER,
    MAIN,
    DIRECT
};

enum class FilterRoute {
    NONE,
    TO_MAIN,
    TO_DIRECT,
    TO_OTHER_FILTER
};

enum class BusRoute {
    NONE,
    TO_MAIN,
    TO_DIRECT,
    TO_OTHER_BUS
};

struct SourceRouteConfig {
    bool enabled = false;
    SourceDest dest = SourceDest::NONE;
    float filterBlend = 0.0f;

    float bus1Send = 0.0f;
    float bus2Send = 0.0f;
};

struct FilterRouteConfig {
    bool enabled = false;
    FilterRoute route = FilterRoute::TO_MAIN;
    float bus1Send = 0.0f;
    float bus2Send = 0.0f;
};
