#pragma once

#include "imgui.h"
#include "ui/UiState.h"

// FX page: a reorderable rack of 8 effects, a large editor for the selected
// one, and the output section (spectrum, scope, master).
namespace fxpage {
void draw(UiState& ui, ImVec2 avail);
}
