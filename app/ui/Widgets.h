#pragma once

#include "imgui.h"
#include "ui/UiState.h"

// Parameter-bound controls. Every control reads and writes the shared
// ParamStore directly (atomics), so there is no UI-side copy to sync.
namespace w {

constexpr const char* MODSRC_PAYLOAD = "HC_MODSRC";

// Rotary knob with modulation rings and a live modulated-value dot.
// Drag (Shift = fine), wheel, double-click reset, Alt+drag = mod depth,
// right-click = menu (MIDI learn, modulation list). Accepts dropped mod sources.
bool Knob(UiState& ui, P p, const char* label = nullptr, float radius = 19.0f);

bool Toggle(UiState& ui, P p, const char* label);          // pill switch
bool Power(UiState& ui, P p, float size = 15.0f);           // round power button
bool Choice(UiState& ui, P p, float width);                 // dropdown
bool Stepper(UiState& ui, P p, float width, const char* label = nullptr);  // number box

// Pill that can be dragged onto any knob to create a modulation route.
void ModSourceHandle(UiState& ui, ModSrc src, float width = 0.0f, const char* label = nullptr);

// Section panel with a header (and optional power switch). Returns false if collapsed/clipped.
bool PanelBegin(UiState& ui, const char* id, const char* title, ImVec2 size, int powerParam = -1);
void PanelEnd();

void SectionLabel(const char* text);
void LevelMeter(float level, ImVec2 size);   // vertical, level in linear gain

int  addModulation(UiState& ui, ModSrc src, int target, float amount);
int  slotsForParam(UiState& ui, int param, int* out, int max);

// helper: draw an arc of a circle
void arc(ImDrawList* dl, ImVec2 c, float r, float a0, float a1, ImU32 col, float thickness);

} // namespace w
