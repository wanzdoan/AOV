#pragma once
// =============================================================================
//  AOV Zygisk — ImGui Menu Module Header
// =============================================================================

struct ImFont;
namespace Menu {
    void setFonts(ImFont* regular, ImFont* bold);
    void toggle();
    bool isVisible();
    void render();
    void applyStyle();
}
