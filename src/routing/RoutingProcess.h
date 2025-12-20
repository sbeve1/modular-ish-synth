#pragma once
#include "Routing.h"
#include "RoutingBuffers.h"
#include "core/Sample.h"

inline Sample routeSource(
    const Sample& s,
    const SourceRouting& r,
    RoutingBuffers& buf)
{
    Sample out{};
    out.left = 0.0f;
    out.right = 0.0f;

    for (int i = 0; i < r.count; ++i) {
        const auto& rt = r.routes[i];

        float l = s.left  * rt.gain;
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
            case LayerTarget::DIRECT:
                out.left  += l;
                out.right += rgt;
                break;
        }
    }

    return out;
}
