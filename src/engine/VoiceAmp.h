#pragma once
#include "core/Sample.h"

struct VoiceAmp {
    inline Sample apply(const Sample& s, float env)
    {
        return { s.left * env, s.right * env };
    }
};
