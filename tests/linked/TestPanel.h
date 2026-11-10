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

    // the panel as ConfigMenu.cpp makes it, in the panel's pixels, and what the framework draws it with
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
            f4cf::imgui::internal::loadCanvasFont(FONT_SIZE);
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
        ImGui::Render();
        answerTextureRequests();

        // ImGui only counts a user error in a Release build, and shows it in a tooltip of the next frame
        CHECK(GImGui->ErrorCountCurrentFrame == 0);
    }
}
