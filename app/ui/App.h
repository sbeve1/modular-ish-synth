#pragma once

#include "ui/UiState.h"

namespace app {

void init(UiState& ui);                    // presets, initial sound
void frame(UiState& ui, bool windowFocused);   // draws everything, handles keys
void loadPreset(UiState& ui, int index);   // factory first, then user files
int  presetCount(const UiState& ui);

} // namespace app
