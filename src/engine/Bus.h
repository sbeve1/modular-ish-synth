#pragma once
#include "core/Sample.h"

struct Bus {
    Sample signal{};
    void clear() { signal = {}; }
};
