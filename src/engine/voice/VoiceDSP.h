#pragma once
#include <algorithm>
#include <cmath>

#include "audio/Filter.h"
#include "engine/FilterParams.h"
#include "routing/Routing.h"
#include "engine/voice/VoiceGraph.h"
#include "core/CONSTANTS.h"

struct VoiceDSP {

    static inline Sample processFilter(
        Filter& filter,
        FilterParams& params,
        int index,
        FilterRoute route,
        VoiceRoutingBuffers& buffers,
        float sr)
    {
        if (route == FilterRoute::NONE)
            return {};

        // Grab and clear input
        Sample in = buffers.filterInput[index];
        buffers.filterInput[index] = {};

        float cutoff = params.cutoffBase + params.cutoffMod;
        float res = std::clamp(params.resBase + params.resMod, 0.0f, 0.99f);

        Sample out{};
        filter.process(in.left, in.right, cutoff, res, sr, out.left, out.right);

        // Makeup gain
        float makeup = 1.0f + 0.6f * res;
        out.left  *= makeup;
        out.right *= makeup;

        // ----------------------------
        // PRIMARY FILTER ROUTING
        // ----------------------------
        switch (route) {
            case FilterRoute::TO_MAIN:
                buffers.main.left  += out.left;
                buffers.main.right += out.right;
                break;

            case FilterRoute::TO_DIRECT:
                buffers.direct.left  += out.left;
                buffers.direct.right += out.right;
                break;

            case FilterRoute::TO_OTHER_FILTER:
                buffers.filterInput[1 - index].left  += out.left;
                buffers.filterInput[1 - index].right += out.right;
                break;

            case FilterRoute::NONE:
                break;
        }

        return out;
    }


    static inline void processBus(
        const Sample& in,
        int busIndex,
        BusRoute route,
        VoiceRoutingBuffers& buf)
    {
        switch (route) {
            case BusRoute::TO_MAIN:
                buf.main.left  += in.left;
                buf.main.right += in.right;
                break;

            case BusRoute::TO_DIRECT:
                buf.direct.left  += in.left;
                buf.direct.right += in.right;
                break;

            case BusRoute::TO_OTHER_BUS:
                if (busIndex == 0) {
                    buf.bus2.left  += in.left;
                    buf.bus2.right += in.right;
                } else {
                    buf.bus1.left  += in.left;
                    buf.bus1.right += in.right;
                }
                break;

            default:
                break;
        }
    }
};
