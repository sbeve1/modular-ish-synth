#pragma once

struct FilterParams {
    // cutoff / tone
    float cutoffBase = 1200.0f;
    float cutoffMod  = 1200.0f;

    float resBase    = 0.0f;
    float resMod     = 0.0f;

    float driveBase  = 0.0f;
    float driveMod   = 0.0f;

    // wet/dry
    float mixBase    = 1.0f;
    float mixMod     = 1.0f;

    // post-filter gain
    float levelBase  = 1.0f;
    float levelMod   = 1.0f;

    // post-filter pan (-1..+1)
    float panBase    = 0.0f;
    float panMod     = 0.0f;
};
