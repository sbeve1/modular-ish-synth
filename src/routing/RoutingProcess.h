#pragma once

#include "Routing.h"
#include "RoutingBuffers.h"
#include "core/Sample.h"

// Single, canonical routing function.
// Writes audio into routing buses.
// Does NOT return a Sample.
inline void routeSource(
    const Sample& s,
    const SourceRouting& r,
    RoutingBuffers& buf)
{
    if (r.count == 0)
        return;

    for (int i = 0; i < r.count; ++i) {
        const auto& rt = r.routes[i];

        float l   = s.left  * rt.gain;
        float rgt = s.right * rt.gain;

        switch (rt.target) {
            case LayerTarget::FILTER0:
                buf.filterInput[0].left  += l;
                buf.filterInput[0].right += rgt;
                break;

            case LayerTarget::FILTER1:
                buf.filterInput[1].left  += l;
                buf.filterInput[1].right += rgt;
                break;

            case LayerTarget::MAIN:
                buf.main.left  += l;
                buf.main.right += rgt;
                break;

            case LayerTarget::DIRECT:
                buf.direct.left  += l;
                buf.direct.right += rgt;
                break;
        }
    }
}
