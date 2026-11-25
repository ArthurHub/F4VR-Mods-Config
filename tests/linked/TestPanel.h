#pragma once

#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "Theme.h"
#include "imgui/ImGuiFonts.h"
#include "ui/FullPanel.h"

// The Full panel in frames of ImGui with no game and no renderer, as the framework's canvas has it in the game.
namespace test_panel
{
    using namespace mods_config;

    // the panel as GameMenu.cpp makes it, in the panel's pixels, and what the framework draws it with
    constexpr float PANEL_WIDTH = theme::PANEL_WIDTH;
    constexpr float PANEL_HEIGHT = theme::PANEL_HEIGHT;
    constexpr float PANEL_BORDER = theme::BORDER_PANEL;
    constexpr float PANEL_RADIUS = theme::RADIUS_PANEL;
    constexpr float PANEL_PADDING = theme::PANEL_PADDING;
    constexpr ImU32 PANEL_BACKGROUND = IM_COL32(15, 15, 15, 217);
    constexpr float FONT_SIZE = theme::FONT_SIZE;
    constexpr float SUPERSAMPLE = 1.5f;

    // where the panel's content starts, from the panel's left and from its top
    constexpr float CONTENT_START = PANEL_PADDING + PANEL_BORDER;

    // a pointer that is on nothing
    constexpr ImVec2 POINTER_AWAY(-1000.0f, -1000.0f);

    /**
     * An ImGui context set up as the framework sets its own up for the menu's panel, with no renderer.
     */
    struct PanelContext
    {
        PanelContext()
        {
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.ConfigInputTrickleEventQueue = false;
            io.DisplaySize = ImVec2(PANEL_WIDTH, PANEL_HEIGHT);
            // ImGui rasterizes the glyphs at the supersample, and takes the textures a renderer makes of them
            io.DisplayFramebufferScale = ImVec2(SUPERSAMPLE, SUPERSAMPLE);
            io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
            ImGui::StyleColorsDark();
            // the panel's style, which FullPanel applies to the game's one context on its first frame: a panel drawn
            // in an earlier test context does not apply it again to this one
            theme::applyStyle(ImGui::GetStyle());
            f4cf::imgui::internal::loadCanvasFont(FONT_SIZE);
            // as GameMenu.cpp gives the framework's controls their look, with nothing held from another test
            controls::get().setStyle(ui::controlsStyle());
            controls::get().reset();
            controls::get().setThumbstickSide(0.0f);
        }

        PanelContext(const PanelContext&) = delete;
        PanelContext& operator=(const PanelContext&) = delete;

        ~PanelContext()
        {
            ImGui::DestroyContext();
        }
    };

    /**
     * What a renderer does with ImGui's texture requests, here with no renderer: a texture stays its pixels,
     * which the render tests read, so a request is only marked done.
     */
    inline void answerTextureRequests()
    {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
            if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) {
                tex->SetTexID(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(tex)));
                tex->SetStatus(ImTextureStatus_OK);
            } else if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames > 0) {
                tex->SetTexID(ImTextureID_Invalid);
                tex->SetStatus(ImTextureStatus_Destroyed);
            }
        }
    }

    /**
     * One frame of the panel, in a window as the framework's canvas makes it: the panel's background and its
     * border, and the content inside its padding. The pointer is where the ray is on the panel, and it is
     * pressed while the trigger is held.
     */
    inline void drawFrame(ui::FullPanel& panel, const ImVec2 pointer, const bool pressed = false)
    {
        constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                                      ImGuiWindowFlags_NoScrollbar;
        constexpr float HALF_BORDER = PANEL_BORDER * 0.5f;
        constexpr float FRINGE_SCALE = 1.0f / SUPERSAMPLE;

        auto& io = ImGui::GetIO();
        io.DeltaTime = 1.0f / 90.0f;
        io.AddMousePosEvent(pointer.x, pointer.y);
        io.AddMouseButtonEvent(ImGuiMouseButton_Left, pressed);
        ImGui::NewFrame();
        // the controls' frame, as the framework's layer begins it before any canvas's content
        controls::get().beginFrame(io.DeltaTime);

        const ImVec2 windowMin(HALF_BORDER, HALF_BORDER);
        const ImVec2 windowMax(PANEL_WIDTH - HALF_BORDER, PANEL_HEIGHT - HALF_BORDER);
        ImGui::SetNextWindowPos(windowMin);
        ImGui::SetNextWindowSize(ImVec2(PANEL_WIDTH - PANEL_BORDER, PANEL_HEIGHT - PANEL_BORDER));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Panel", nullptr, WINDOW_FLAGS);
        ImGui::PopStyleVar(2);

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->_FringeScale = FRINGE_SCALE;
        drawList->AddRectFilled(windowMin, windowMax, PANEL_BACKGROUND, PANEL_RADIUS - HALF_BORDER);
        drawList->AddRect(windowMin, windowMax, theme::color::GREEN, PANEL_RADIUS - HALF_BORDER, PANEL_BORDER);

        ImGui::SetCursorPos(ImVec2(PANEL_PADDING + HALF_BORDER, PANEL_PADDING + HALF_BORDER));
        const ImVec2 contentSize(PANEL_WIDTH - (PANEL_PADDING + PANEL_BORDER) * 2.0f, PANEL_HEIGHT - (PANEL_PADDING + PANEL_BORDER) * 2.0f);
        if (ImGui::BeginChild("content", contentSize, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
            ImGui::GetWindowDrawList()->_FringeScale = FRINGE_SCALE;
            ImGui::BeginGroup();
            panel.draw();
            ImGui::EndGroup();
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleColor();
        controls::get().endFrame();
        ImGui::Render();
        answerTextureRequests();

        // ImGui only counts a user error in a Release build, and shows it in a tooltip of the next frame
        CHECK(GImGui->ErrorCountCurrentFrame == 0);
    }

    // the tree's header, the search field, as ModTree.cpp has it
    constexpr float TREE_HEADER_HEIGHT = 40.0f;

    inline void drawFrames(ui::FullPanel& panel, const int count)
    {
        for (int i = 0; i < count; ++i) {
            drawFrame(panel, POINTER_AWAY);
        }
    }

    /**
     * A press at a place on the panel. Returns whether it acted in one of its frames.
     */
    inline bool pressActs(ui::FullPanel& panel, const ImVec2 at)
    {
        bool acted = false;
        for (const bool pressed : { false, false, true, false, false }) {
            drawFrame(panel, at, pressed);
            acted = acted || panel.pressActed();
        }
        return acted;
    }

    /**
     * The ray at a place on the panel, for the frames ImGui needs to find what it is on.
     */
    inline void point(ui::FullPanel& panel, const ImVec2 at)
    {
        for (int i = 0; i < 3; ++i) {
            drawFrame(panel, at);
        }
    }

    /**
     * The middle of a row of a page, by its place among the rows, while the page is at its top and has rows of
     * controls alone.
     */
    inline ImVec2 pageRow(const int place)
    {
        const float rowsTop = CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3;
        return { CONTENT_START + theme::NAV_WIDTH + theme::SPACE_4 + 200.0f, rowsTop + static_cast<float>(place) * theme::ROW_PITCH + theme::CONTROL_HEIGHT * 0.5f };
    }

    /**
     * Characters typed into the field that is edited, with the ray on no row, as the keys of the window and
     * SteamVR's keyboard type them.
     */
    inline void typeText(ui::FullPanel& panel, const char* text)
    {
        ImGui::GetIO().AddInputCharactersUTF8(text);
        drawFrame(panel, POINTER_AWAY);
    }

    /**
     * A key pressed in one frame and let go of in the next, as the framework sends one.
     */
    inline void pressKey(ui::FullPanel& panel, const ImGuiKey key)
    {
        for (const bool down : { true, false }) {
            ImGui::GetIO().AddKeyEvent(key, down);
            drawFrame(panel, POINTER_AWAY);
        }
    }

    /**
     * The middle of the search field, the tree's header.
     */
    inline ImVec2 searchField()
    {
        return { CONTENT_START + theme::SPACE_2 + 100.0f, CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3 + theme::SPACE_2 + TREE_HEADER_HEIGHT * 0.5f };
    }

    /**
     * A query typed in the search field: a press on it, its characters, and Enter.
     */
    inline void search(ui::FullPanel& panel, const char* query)
    {
        CHECK(pressActs(panel, searchField()));
        typeText(panel, query);
        pressKey(panel, ImGuiKey_Enter);
    }
}
