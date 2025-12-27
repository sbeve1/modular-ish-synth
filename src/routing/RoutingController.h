#pragma once
#include "engine/voice/VoiceRouting.h"
#include "routing/RoutingIDs.h"

struct RoutingController {

    static void setSourceDest(
        VoiceRouting& r,
        SourceID src,
        SourceDest dest)
    {
        r.sources[(int)src].dest = dest;
        r.compile();
    }

    static void setSourceBusSend(
        VoiceRouting& r,
        SourceID src,
        BusID bus,
        float amount)
    {
        amount = std::clamp(amount, 0.0f, 1.0f);

        if (bus == BusID::BUS1)
            r.sources[(int)src].bus1Send = amount;
        else
            r.sources[(int)src].bus2Send = amount;

        r.compile();
    }

    static void setFilterRoute(
        VoiceRouting& r,
        FilterID f,
        FilterRoute route)
    {
        r.filter[(int)f].route = route;
        r.compile();
    }

    static void setBusRoute(
        VoiceRouting& r,
        BusID bus,
        BusRoute route)
    {
        r.busRoute[(int)bus] = route;
        r.compile();
    }

    static void setSourceFilterBlend(
        VoiceRouting& r,
        SourceID src,
        float blend)
    {
        r.sources[(int)src].filterBlend =
            std::clamp(blend, 0.0f, 1.0f);
        r.compile();
    }
};
