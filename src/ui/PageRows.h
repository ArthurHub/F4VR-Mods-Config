#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    // The marks after a row's name, in this order: the Restart tag, the pending dot, then the warning mark or
    // the invalid mark.
    struct RowMarks
    {
        bool restart = false;
        bool pending = false;
        TextMark mark = TextMark::None;
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
    Row addRow(bool pressable);
    void drawLabel(const ImVec2& pos, float right, std::string_view text, std::span<const model::TextRun> runs, const RowMarks& marks = {});
    std::string_view warningOf(const std::vector<model::Warning>& warnings, model::Store* store);
    std::string readOnlyHelp(std::string reason, const std::string& help);
}
