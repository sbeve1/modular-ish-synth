#pragma once

#include "imgui.h"

// Halcyon visual language: deep blue-black panels, cyan for values,
// violet for modulation, amber for warnings.
namespace theme {

inline ImU32 rgba(int r, int g, int b, int a = 255) { return IM_COL32(r, g, b, a); }

inline const ImU32 bg          = rgba(11, 13, 19);
inline const ImU32 panel       = rgba(20, 24, 33);
inline const ImU32 panelHi     = rgba(27, 32, 44);
inline const ImU32 border      = rgba(40, 47, 63);
inline const ImU32 track       = rgba(38, 44, 58);
inline const ImU32 knobBody    = rgba(30, 35, 47);
inline const ImU32 knobBodyHi  = rgba(44, 51, 68);
inline const ImU32 text        = rgba(214, 222, 235);
inline const ImU32 textDim     = rgba(128, 140, 160);
inline const ImU32 textFaint   = rgba(80, 90, 108);
inline const ImU32 accent      = rgba(64, 224, 208);    // cyan
inline const ImU32 accentDim   = rgba(64, 224, 208, 90);
inline const ImU32 accentFaint = rgba(64, 224, 208, 28);
inline const ImU32 mod         = rgba(176, 124, 255);   // violet
inline const ImU32 modDim      = rgba(176, 124, 255, 110);
inline const ImU32 modFaint    = rgba(176, 124, 255, 30);
inline const ImU32 warn        = rgba(255, 181, 71);
inline const ImU32 hot         = rgba(255, 92, 110);
inline const ImU32 keyWhite    = rgba(222, 226, 234);
inline const ImU32 keyBlack    = rgba(24, 27, 36);

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
