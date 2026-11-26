// Generated from the design system in docs/design/design-system by docs/design/design-system/tools/gen_theme.py. Do not edit it:
// change project/tokens.json, or the tables of the "ImGui style" section of project/README.md, and run the generator.
// pre-commit runs it on every commit that changes the design system.
#pragma once

#include <imgui.h>

#include "imgui/ImGuiTheme.h"

// clang-format off
namespace mods_config::theme
{
    // Colors, as ImU32 for ImDrawList. ImGui::ColorConvertU32ToFloat4 makes the ImVec4 an ImGuiStyle takes.
    namespace color
    {
        constexpr ImU32 GLASS        = IM_COL32( 15,  15,  15, 217); // Background of the Full panel (UIPanelStyle.background, alpha 217 = 0.85)
        constexpr ImU32 GLASS_WIDGET = IM_COL32( 15,  15,  15, 166); // Exact F4VR_PANEL_STYLE / F4VR_BUTTON_STYLE background
        constexpr ImU32 POPUP        = IM_COL32( 11,  18,  14, 245); // ImGuiCol_PopupBg: dropdown lists and the confirmation dialog, nearly opaque so the rows under them do not show through
        constexpr ImU32 SCRIM        = IM_COL32(  5,   8,   6, 140); // ImGuiCol_ModalWindowDimBg: dims the panel behind a confirmation dialog
        constexpr ImU32 PLATE        = IM_COL32( 10, 250, 120,  15); // Green at 6%: the navigation pane, so it reads as a fixed region
        constexpr ImU32 TRACK        = IM_COL32( 10, 250, 120,  26); // Green at 10%: ImGuiCol_FrameBg
        constexpr ImU32 HOVER        = IM_COL32( 10, 250, 120,  31); // Green at 12%: a row or control under the ray (ImGuiCol_HeaderHovered, ButtonHovered, FrameBgHovered)
        constexpr ImU32 PRESSED      = IM_COL32( 10, 250, 120,  51); // Green at 20%: trigger held on a control (ImGuiCol_HeaderActive, ButtonActive, FrameBgActive)
        constexpr ImU32 RULE         = IM_COL32( 10, 250, 120, 102); // Green at 40%: section rules and separators (ImGuiCol_Separator)
        constexpr ImU32 FRAME_BORDER = IM_COL32( 10, 250, 120, 140); // Green at 55%: ImGuiCol_Border
        constexpr ImU32 GREEN        = IM_COL32( 10, 250, 120, 255); // The house color, rgb 10,250,120, from F4VR_PANEL_STYLE
        constexpr ImU32 GREEN_MUTED  = IM_COL32(159, 224, 187, 255); // Secondary text: help, page help, units, counts, the session status (ImGuiCol_TextDisabled)
        constexpr ImU32 GLOW         = IM_COL32(196, 255, 220, 255); // The pressed outline and the slider grab while dragging (ImGuiCol_SliderGrabActive)
        constexpr ImU32 ON_GREEN     = IM_COL32(  4,  23,  12, 255); // Text and drawn icons on a solid green fill: the selected tree row, the selected half of a toggle, the current option, the Save button
        constexpr ImU32 AMBER        = IM_COL32(255, 191,  51, 255); // Pending: a setting that differs from its saved value
        constexpr ImU32 ON_AMBER     = IM_COL32( 31,  20,   0, 255); // Text on an amber fill, if one is ever needed
        constexpr ImU32 RED          = IM_COL32(255, 149, 133, 255); // Invalid: the value text and the gutter mark of a setting that cannot be saved, and the error in the help line
        constexpr ImU32 ON_RED       = IM_COL32( 36,   8,   5, 255); // The "!" inside the drawn warning triangle, which is filled red
        constexpr ImU32 ACCENT       = IM_COL32( 95, 212, 255, 255); // An important button: the outline, label and icon of a button whose config spec gives it style important, so the player does not miss it
        constexpr ImU32 DISABLED     = IM_COL32( 10, 250, 120, 102); // Text and outlines of a disabled row or control, which is ImGui's DisabledAlpha 0.4 applied to green
        constexpr ImU32 RAY          = IM_COL32(190, 255, 190, 200); // The pointer ray: cRayColor = { rgb = [190, 255, 190], alpha = 200 } in [Pointer] of ModsConfigFO4VR.toml
        constexpr ImU32 MARK         = IM_COL32(215, 255, 215, 180); // The mark where the ray meets the panel: cMarkColor = { rgb = [215, 255, 215], alpha = 180 }
        constexpr ImU32 MARK_BORDER  = IM_COL32(215, 255, 215, 100); // The border of the mark: cMarkBorderColor = { rgb = [215, 255, 215], alpha = 100 }
    }

    // Spacing, in the panel's layout px: 48 px is one vrui unit.
    constexpr float SPACE_1           = 4.0f;     // TouchExtraPadding on each side: widens hit boxes into the item gap without moving anything
    constexpr float SPACE_2           = 8.0f;     // ItemSpacing.y and ItemInnerSpacing: between rows, between a control's parts
    constexpr float SPACE_3           = 12.0f;    // FramePadding.y and ItemSpacing.x
    constexpr float SPACE_4           = 16.0f;    // FramePadding.x, WindowPadding of the content pane and of popups, the gap between the navigation and content panes, a level of the tree, as a page...
    constexpr float SPACE_6           = 24.0f;    // The row gutter that holds the pending dot or the warning mark; space above a section heading
    constexpr float SPACE_8           = 32.0f;    // IndentSpacing: a level of the content, as a section in a section
    constexpr float SPACE_12          = 48.0f;    // One vrui unit

    // Corner radii, in layout px.
    constexpr float RADIUS_PANEL      = 24.0f;    // The Full panel's corner, cornerRadiusUnits 0.5 in GameMenu.cpp
    constexpr float RADIUS_WIDGET     = 9.6f;     // F4VR_BUTTON_STYLE cornerRadiusUnits 0.2: the vrui wrist buttons
    constexpr float RADIUS_POPUP      = 8.0f;     // PopupRounding, ChildRounding, WindowRounding of the dialog
    constexpr float RADIUS_FRAME      = 4.0f;     // FrameRounding and ScrollbarRounding: controls, buttons, rows
    constexpr float RADIUS_GRAB       = 2.0f;     // GrabRounding: the slider grab

    // Sizes, in layout px.
    constexpr float PANEL_WIDTH       = 1152.0f;  // Full panel canvas width: PANEL_WIDTH 24 units
    constexpr float PANEL_HEIGHT      = 720.0f;   // Full panel canvas height: PANEL_HEIGHT 15 units
    constexpr float PANEL_PADDING     = 14.4f;    // UIPadding::all(0.3) in GameMenu.cpp: the canvas inset inside the Full panel's border
    constexpr float CONTROL_HEIGHT    = 48.0f;    // Every pressable control and row, drawn at this height: not ImGui's frame height
    constexpr float ROW_PITCH         = 56.0f;    // Control height plus ItemSpacing.y: the distance from one setting row to the next
    constexpr float TITLEBAR_HEIGHT   = 64.0f;    // The Full panel's title bar
    constexpr float FOOTER_HEIGHT     = 72.0f;    // The footer: Discard and Save on the right; on the left the help line (two t-body lines or three t-detail lines) or the session status
    constexpr float NAV_WIDTH         = 336.0f;   // The navigation tree (7 units): the same place and width with or without a selected mod
    constexpr float CONTROL_WIDTH     = 448.0f;   // The right-hand control column of a setting row
    constexpr float VALUE_WIDTH       = 104.0f;   // The value box of a slider or stepper: room for -1000.00 in t-body
    constexpr float GRAB_WIDTH        = 20.0f;    // GrabMinSize: the slider grab, 20 x 40
    constexpr float SCROLLBAR_SIZE    = 16.0f;    // ScrollbarSize

    // Border widths, in layout px.
    constexpr float BORDER_PANEL      = 3.84f;    // The Full panel's edge: borderThicknessUnits 0.08 in GameMenu.cpp, in green
    constexpr float BORDER_FRAME      = 2.0f;     // FrameBorderSize, PopupBorderSize, SeparatorTextBorderSize, the hover outline and the focus ring
    constexpr float BORDER_WIDGET     = 2.4f;     // F4VR_BUTTON_STYLE borderThicknessUnits 0.05 on the wrist buttons

    // Opacity.
    constexpr float OPACITY_DISABLED  = 0.4f;     // ImGuiStyle.DisabledAlpha, applied by BeginDisabled to a whole row
    constexpr float OPACITY_OTHER_RAY = 0.3f;     // The ray of the hand that does not own the pointer, drawn by the framework with no mark

    // Font sizes for ImGui::PushFont(nullptr, size) around the call, and PopFont after it. t-body is the canvas font's.
    constexpr float FONT_SIZE_TITLE   = 26.0f;      // 26 px
    constexpr float FONT_SIZE_BODY    = 21.0f;      // 21 px
    constexpr float FONT_SIZE_DETAIL  = 18.0f;      // 18 px

    /**
     * A color token as the framework's color.
     */
    constexpr f4cf::render::Color toColor(const ImU32 color)
    {
        return f4cf::render::Color::rgba(static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFF), static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFF),
            static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFF), static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF));
    }

    /**
     * The design system as the framework's theme, every field from its token: the controls, ImGui's style, the
     * keyboard and the panel's edge are drawn in it. A panel's sizes in vrui units are their px over the 48 of a unit.
     */
    inline f4cf::imgui::Theme designTheme()
    {
        return {
            .accent            = toColor(color::GREEN),
            .muted             = toColor(color::GREEN_MUTED),
            .track             = toColor(color::TRACK),
            .hover             = toColor(color::HOVER),
            .pressed           = toColor(color::PRESSED),
            .glow              = toColor(color::GLOW),
            .rule              = toColor(color::RULE),
            .frameBorder       = toColor(color::FRAME_BORDER),
            .onAccent          = toColor(color::ON_GREEN),
            .warning           = toColor(color::AMBER),
            .onWarning         = toColor(color::ON_AMBER),
            .error             = toColor(color::RED),
            .onError           = toColor(color::ON_RED),
            .popup             = toColor(color::POPUP),
            .scrim             = toColor(color::SCRIM),
            .glass             = toColor(color::GLASS),
            .plate             = toColor(color::PLATE),
            .important         = toColor(color::ACCENT),
            .controlHeight     = CONTROL_HEIGHT,
            .frameRounding     = RADIUS_FRAME,
            .frameBorderSize   = BORDER_FRAME,
            .popupRounding     = RADIUS_POPUP,
            .titleFontSize     = FONT_SIZE_TITLE,
            .bodyFontSize      = FONT_SIZE_BODY,
            .detailFontSize    = FONT_SIZE_DETAIL,
            .scrollbarSize     = SCROLLBAR_SIZE,
            .grabWidth         = GRAB_WIDTH,
            .grabRounding      = RADIUS_GRAB,
            .disabledOpacity   = OPACITY_DISABLED,
            .space1            = SPACE_1,
            .space2            = SPACE_2,
            .space3            = SPACE_3,
            .space4            = SPACE_4,
            .space6            = SPACE_6,
            .space8            = SPACE_8,
            .space12           = SPACE_12,
            .panelBorderUnits  = BORDER_PANEL / 48.0f,
            .panelCornerUnits  = RADIUS_PANEL / 48.0f,
            .panelPaddingUnits = PANEL_PADDING / 48.0f,
        };
    }

    // A framework whose Theme has another field than the generator read stops the build here, so the mod never takes
    // a default for it: run the generator.
    static_assert(sizeof(f4cf::imgui::Theme) == 372, "imgui::Theme has changed: run docs/design/design-system/tools/gen_theme.py");
}

// clang-format on
