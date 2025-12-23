#pragma once

#include <algorithm>
#include <cmath>

#include "audio/Oscillator.h"
#include "audio/Noise.h"
#include "audio/Filter.h"
#include "engine/FilterParams.h"
#include "engine/NoiseParams.h"
#include "routing/Routing.h"
#include "core/CONSTANTS.h"

struct VoiceDSP {

    // ----------------------------
    // FILTER PROCESS
    // ----------------------------
    static inline void processFilter(
        Filter& filter,
        FilterParams& params,
        int index,
        FilterRoute route,
        VoiceRoutingBuffers& buffers,
        float sr)
    {
        if (route == FilterRoute::NONE)
            return;

        auto& in = buffers.filterInput[index];

        float cutoff = params.cutoffBase + params.cutoffMod;
        float res    = std::clamp(params.resBase + params.resMod, 0.0f, 0.99f);

        float outL = 0.0f;
        float outR = 0.0f;

        filter.process(
            in.left,
            in.right,
            cutoff,
            res,
            sr,
            outL,
            outR
        );

        // Serum-style loudness compensation
        float makeup = 1.0f + 0.6f * res + 0.3f * res * res;
        outL *= makeup;
        outR *= makeup;

        float drive = std::max(params.driveBase + params.driveMod, 0.0f);
        if (drive > 0.0f) {
            float d = 1.0f + drive * 2.0f;
            outL = std::tanh(outL * d);
            outR = std::tanh(outR * d);
        }

        float mix = std::clamp(params.mixBase + params.mixMod, 0.0f, 1.0f);
        outL = outL * mix + in.left  * (1.0f - mix);
        outR = outR * mix + in.right * (1.0f - mix);

        float level = std::max(params.levelBase + params.levelMod, 0.0f);
        outL *= level;
        outR *= level;

        float pan = std::clamp(params.panBase + params.panMod, -1.0f, 1.0f);
        float ang = (pan + 1.0f) * 0.25f * PI;

        outL *= std::cos(ang);
        outR *= std::sin(ang);

        switch (route) {
            case FilterRoute::TO_FX:
                buffers.fx.left  += outL;
                buffers.fx.right += outR;
                break;
            case FilterRoute::TO_DIRECT:
                buffers.direct.left  += outL;
                buffers.direct.right += outR;
                break;
            case FilterRoute::TO_OTHER_FILTER:
                buffers.filterInput[1 - index].left  += outL;
                buffers.filterInput[1 - index].right += outR;
                break;
            case FilterRoute::NONE:
                break;
        }
    }
};
