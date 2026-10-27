#pragma once

#include "imgui.h"
#include "ui/UiState.h"

// Graphical editors and visualizations. All read audio state through
// Telemetry (atomics) and write through the ParamStore; none of them touch
// the audio thread directly.
namespace views {

// 3D wavetable: frames recede into depth, the playing position glows.
// Drag to rotate, double-click to toggle 2D/3D.
void WavetableView(UiState& ui, int osc, ImVec2 size, const WavetableSet* overrideTable = nullptr, float overridePos = -1.0f);

void EnvelopeEditor(UiState& ui, int env, ImVec2 size);   // draggable nodes and curves
void LfoGraph(UiState& ui, int lfo, ImVec2 size);         // shape + live phase
void FilterResponse(UiState& ui, ImVec2 size);            // response curve, drag to edit
void Oscilloscope(UiState& ui, ImVec2 size);
void Spectrum(UiState& ui, ImVec2 size);
void Piano(UiState& ui, ImVec2 size, int lowNote, int highNote);
void Wheels(UiState& ui, ImVec2 size);                    // pitch bend + mod wheel

void LfoShapeIcon(ImDrawList* dl, int shape, ImVec2 a, ImVec2 b, ImU32 col);

} // namespace views
