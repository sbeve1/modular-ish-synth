#pragma once

#include "ui/UiState.h"

// Wavetable editor window: draw frames, generate shapes and harmonics,
// morph between frames. Every committed edit rebuilds the band-limited
// table and swaps it into the oscillator, so changes are heard immediately.
namespace wtedit {
void draw(UiState& ui);
}
