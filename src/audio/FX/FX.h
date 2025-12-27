#pragma once
#include "core/Sample.h"

struct FX {
    virtual ~FX() = default;
    virtual Sample process(const Sample& in, float sr) = 0;
};
