#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

namespace mods_config::ui
{
    /**
     * What a target of the panel is, which says what its key is.
     */
    enum class TargetKind : std::uint8_t
    {
        // a mod's row of the tree, or a page's: the key is the mod's name, and the path the names of the pages to it
        Tree,
        // a category's row of the tree, by its name
        Category,
        // a setting's row of the page, by its id
        Setting,
        // a button's row of the page, by its text
        PageButton,
        // a section's heading on the page, by its text
        Section,
        // a button of the title bar, by its name: Help, Minimize, Close
        TitleButton,
        // a breadcrumb of the title bar, by its text
        Crumb,
        // a button with a label, as a dialog's and an editor's footer have, by its label
        Button,
        // the track of a channel of the color editor that is open, by the channel's name
        ColorChannel,
    };

    std::string_view targetKindName(TargetKind kind);

    /**
     * A place of the panel the ray can be put on, as it was drawn: its rect in the panel's pixels from its top left,
     * the part of it that was shown.
     */
    struct Target
    {
        TargetKind kind = TargetKind::Tree;
        std::string key;
        std::vector<std::string> path;
        ImRect rect;
    };

    /**
     * The targets of the panel's last frame, which the mod's devbench tool tells so a test with the controllers knows
     * where to point. Nothing is kept until recording is turned on, so a game with no test pays one check a target.
     */
    class PanelTargets
    {
    public:
        static PanelTargets& get();

        void setRecording(const bool recording)
        {
            _recording = recording;
        }

        bool isRecording() const
        {
            return _recording;
        }

        const std::vector<Target>& targets() const
        {
            return _targets;
        }

        // Internal: the panel and the parts it draws
        void startFrame();
        void add(TargetKind kind, std::string_view key, const ImRect& rect, std::vector<std::string> path = {});

    private:
        PanelTargets() = default;

        bool _recording = false;

        // the panel's top left in ImGui's display, which a target is from
        ImVec2 _origin;

        std::vector<Target> _targets;
    };

    inline PanelTargets& panelTargets()
    {
        return PanelTargets::get();
    }
}
