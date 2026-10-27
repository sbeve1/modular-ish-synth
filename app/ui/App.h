#pragma once

#include "ui/UiState.h"

namespace app {

void init(UiState& ui);                    // presets, initial sound
void frame(UiState& ui, bool windowFocused);   // draws everything, handles keys
void loadPreset(UiState& ui, int index);   // factory first, then user files
void onKey(UiState& ui, int glfwKey, int action, int ctrlOrAlt);   // from the GLFW key callback
void releaseKeys(UiState& ui);
int  presetCount(const UiState& ui);

} // namespace app
