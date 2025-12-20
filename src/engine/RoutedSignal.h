#pragma once
#include "core/Sample.h"

struct RoutedSignal {
    Sample direct{};
    Sample main{};
    Sample bus1{};
    Sample bus2{};
};
