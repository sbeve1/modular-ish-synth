#pragma once

#include "imgui.h"

// modular_synth visual language: near-black neutrals, white text, one green
// accent (neon green) for values, white for modulation. Flat and minimal.
namespace theme {

inline ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

inline const ImU32 bg          = rgba(8, 8, 9);
inline const ImU32 panel       = rgba(14, 14, 15);
inline const ImU32 panelHi     = rgba(18, 18, 20);
inline const ImU32 border      = rgba(36, 36, 39);
inline const ImU32 track       = rgba(38, 38, 41);
inline const ImU32 knobBody    = rgba(22, 22, 24);
inline const ImU32 knobBodyHi  = rgba(32, 32, 35);
inline const ImU32 text        = rgba(236, 236, 238);
inline const ImU32 textDim     = rgba(138, 138, 144);
inline const ImU32 textFaint   = rgba(78, 78, 84);
inline const ImU32 accent      = rgba(0, 255, 65);       // neon green: values
inline const ImU32 accentDim   = rgba(0, 255, 65, 90);
inline const ImU32 accentFaint = rgba(0, 255, 65, 24);
inline const ImU32 mod         = rgba(245, 245, 245);    // white: modulation
inline const ImU32 modDim      = rgba(245, 245, 245, 120);
inline const ImU32 modFaint    = rgba(245, 245, 245, 28);
inline const ImU32 warn        = rgba(255, 255, 255);
inline const ImU32 hot         = rgba(255, 255, 255);
inline const ImU32 keyWhite    = rgba(226, 226, 230);
inline const ImU32 keyBlack    = rgba(12, 12, 14);

inline ImU32 withAlpha(ImU32 c, float a)
{
    ImU32 alpha = (ImU32)(((c >> IM_COL32_A_SHIFT) & 0xFF) * a);
    return (c & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

inline ImU32 lerpColor(ImU32 a, ImU32 b, float t)
{
    ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t,
                                                 x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}

void apply();                        // ImGui style
void loadFonts(float scale);         // Segoe UI if available
extern ImFont* fontSmall;
extern ImFont* fontBody;
extern ImFont* fontBold;
extern ImFont* fontTitle;

} // namespace theme
