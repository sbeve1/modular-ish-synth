#include "ui/Theme.h"

#include <filesystem>

namespace theme {

ImFont* fontSmall = nullptr;
ImFont* fontBody  = nullptr;
ImFont* fontBold  = nullptr;
ImFont* fontTitle = nullptr;

void loadFonts(float scale)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const char* regular = "C:\\Windows\\Fonts\\segoeui.ttf";
    const char* bold    = "C:\\Windows\\Fonts\\seguisb.ttf";
    bool haveRegular = std::filesystem::exists(regular);
    bool haveBold    = std::filesystem::exists(bold);

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;

    auto load = [&](const char* path, bool have, float px) {
        if (have) return io.Fonts->AddFontFromFileTTF(path, px * scale, &cfg);
        ImFontConfig d;
        d.SizePixels = px * scale;
        return io.Fonts->AddFontDefault(&d);
    };

    fontBody  = load(regular, haveRegular, 15.0f);   // first font = default
    fontSmall = load(regular, haveRegular, 12.5f);
    fontBold  = load(bold, haveBold, 14.0f);
    fontTitle = load(bold, haveBold, 21.0f);
}

void apply()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.ChildRounding = 6.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 6.0f;
    s.GrabRounding = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.TabRounding = 4.0f;
    s.WindowPadding = ImVec2(10, 10);
    s.FramePadding = ImVec2(8, 4);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize = 10.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    auto f = [](ImU32 u) { return ImGui::ColorConvertU32ToFloat4(u); };
    c[ImGuiCol_Text]                 = f(text);
    c[ImGuiCol_TextDisabled]         = f(textDim);
    c[ImGuiCol_WindowBg]             = f(bg);
    c[ImGuiCol_ChildBg]              = f(panel);
    c[ImGuiCol_PopupBg]              = f(rgba(22, 26, 36, 250));
    c[ImGuiCol_Border]               = f(border);
    c[ImGuiCol_FrameBg]              = f(rgba(32, 38, 51));
    c[ImGuiCol_FrameBgHovered]       = f(rgba(42, 49, 66));
    c[ImGuiCol_FrameBgActive]        = f(rgba(50, 58, 78));
    c[ImGuiCol_TitleBg]              = f(panel);
    c[ImGuiCol_TitleBgActive]        = f(panelHi);
    c[ImGuiCol_MenuBarBg]            = f(panel);
    c[ImGuiCol_ScrollbarBg]          = f(rgba(0, 0, 0, 0));
    c[ImGuiCol_ScrollbarGrab]        = f(rgba(52, 60, 80));
    c[ImGuiCol_ScrollbarGrabHovered] = f(rgba(66, 76, 100));
    c[ImGuiCol_ScrollbarGrabActive]  = f(accentDim);
    c[ImGuiCol_CheckMark]            = f(accent);
    c[ImGuiCol_SliderGrab]           = f(accent);
    c[ImGuiCol_SliderGrabActive]     = f(rgba(120, 240, 228));
    c[ImGuiCol_Button]               = f(rgba(34, 40, 54));
    c[ImGuiCol_ButtonHovered]        = f(rgba(46, 54, 72));
    c[ImGuiCol_ButtonActive]         = f(rgba(56, 66, 88));
    c[ImGuiCol_Header]               = f(rgba(40, 48, 64));
    c[ImGuiCol_HeaderHovered]        = f(rgba(48, 58, 78));
    c[ImGuiCol_HeaderActive]         = f(rgba(58, 70, 94));
    c[ImGuiCol_Separator]            = f(border);
    c[ImGuiCol_Tab]                  = f(rgba(28, 33, 45));
    c[ImGuiCol_TabHovered]           = f(rgba(46, 54, 72));
    c[ImGuiCol_TabSelected]          = f(rgba(40, 48, 64));
    c[ImGuiCol_TableHeaderBg]        = f(rgba(26, 31, 42));
    c[ImGuiCol_TableBorderStrong]    = f(border);
    c[ImGuiCol_TableBorderLight]     = f(rgba(32, 38, 51));
    c[ImGuiCol_TableRowBg]           = f(rgba(0, 0, 0, 0));
    c[ImGuiCol_TableRowBgAlt]        = f(rgba(255, 255, 255, 6));
    c[ImGuiCol_TextSelectedBg]       = f(accentDim);
    c[ImGuiCol_DragDropTarget]       = f(mod);
    c[ImGuiCol_NavHighlight]         = f(accent);
    c[ImGuiCol_ModalWindowDimBg]     = f(rgba(0, 0, 0, 140));
}

} // namespace theme
