#pragma once
#include "VoiceSources.h"
#include "RoutedSignal.h"
#include "SourceRouting.h"

struct SourceRouter {
    SourceRouting oscA;
    SourceRouting oscB;
    SourceRouting oscC;
    SourceRouting sub;
    SourceRouting noise;

    inline void routeOne(const Sample& s,
                         const SourceRouting& r,
                         RoutedSignal& out)
    {
        out.direct.left += s.left * r.toDirect;
        out.direct.right += s.right * r.toDirect;

        out.main.left += s.left * r.toMain;
        out.main.right += s.right * r.toMain;

        out.bus1.left += s.left * r.toBus1;
        out.bus1.right += s.right * r.toBus1;

        out.bus2.left += s.left * r.toBus2;
        out.bus2.right += s.right * r.toBus2;
    }

    RoutedSignal route(const VoiceSources& v)
    {
        RoutedSignal out{};
        routeOne(v.oscA, oscA, out);
        routeOne(v.oscB, oscB, out);
        routeOne(v.oscC, oscC, out);
        routeOne(v.sub,  sub,  out);
        routeOne(v.noise,noise,out);
        return out;
    }
};
