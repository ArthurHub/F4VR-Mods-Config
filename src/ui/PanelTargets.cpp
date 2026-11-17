#include "PanelTargets.h"

#include "Theme.h"

namespace mods_config::ui
{
    /**
     * The name of a kind of target, as the mod's devbench tool tells it.
     */
    std::string_view targetKindName(const TargetKind kind)
    {
        switch (kind) {
        case TargetKind::Tree:
            return "tree";
        case TargetKind::Category:
            return "category";
        case TargetKind::Setting:
            return "setting";
        case TargetKind::PageButton:
            return "pageButton";
        case TargetKind::Section:
            return "section";
        case TargetKind::TitleButton:
            return "titleButton";
        case TargetKind::Crumb:
            return "crumb";
        case TargetKind::Button:
            return "button";
        case TargetKind::ColorChannel:
            return "colorChannel";
        }
        return "unknown";
    }

    PanelTargets& PanelTargets::get()
    {
        static PanelTargets instance;
        return instance;
    }

    /**
     * Starts the targets of a frame of the panel, from the window it is drawn in. The canvas's window stands half
     * the panel's border inside the canvas, so the panel's pixels start that far before it.
     */
    void PanelTargets::startFrame()
    {
        _targets.clear();
        if (!_recording) {
            return;
        }
        const auto* window = ImGui::GetCurrentWindow();
        const auto pos = window ? window->RootWindow->Pos : ImVec2();
        _origin = ImVec2(pos.x - theme::BORDER_PANEL * 0.5f, pos.y - theme::BORDER_PANEL * 0.5f);
    }

    /**
     * Adds a target of the window that is drawn, in ImGui's display: the part of it the window shows is kept, and
     * one the window does not show, as a row scrolled away, is not.
     */
    void PanelTargets::add(const TargetKind kind, const std::string_view key, const ImRect& rect, std::vector<std::string> path)
    {
        if (!_recording) {
            return;
        }
        ImRect shown = rect;
        if (const auto* window = ImGui::GetCurrentWindow()) {
            shown.ClipWithFull(window->ClipRect);
        }
        if (shown.GetWidth() <= 0.0f || shown.GetHeight() <= 0.0f) {
            return;
        }
        shown.Translate(ImVec2(-_origin.x, -_origin.y));
        _targets.push_back({ .kind = kind, .key = std::string(key), .path = std::move(path), .rect = shown });
    }
}
