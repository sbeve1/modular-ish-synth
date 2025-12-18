#pragma once
#include "Voice.h"   // Voice must be known BEFORE writing resolvers

inline float* resolveVcaGain(Voice& v) { return v.vca.getGainPtr(); }
inline float* resolveFilterCutoff(Voice& v) { return v.filter.getCutoffPtr(); }
inline float* resolveOscFrequency(Voice& v) { return v.osc.getFreqPtr(); }
