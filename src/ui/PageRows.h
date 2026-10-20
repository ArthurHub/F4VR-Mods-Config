#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "Theme.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    // A row's gutter: its width, the mark's center, and the inset for text and images without a label.
    struct Gutter
    {
        float width = theme::SPACE_6;
        float mark = theme::SPACE_6 * 0.5f;
        float inset = theme::SPACE_2;
    };

    // Visibility and pointer interactions for a row added to the page.
    struct Row
    {
        bool shown = false;
        bool pointed = false;
        bool started = false;
        bool held = false;
        bool pressed = false;
    };

    float rowWidth();
    Gutter gutterAt(int depth);
    Row addRow(bool pressable);
    void drawLabel(const ImVec2& pos, const Gutter& gutter, float right, std::string_view text, std::span<const model::TextRun> runs, bool warned = false, bool restart = false);
    std::string_view warningOf(const std::vector<model::Warning>& warnings, model::Store* store);
    std::string readOnlyHelp(std::string reason, const std::string& help);
}
