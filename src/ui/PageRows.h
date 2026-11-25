#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    // The marks after a row's name, in this order: the pending mark, the dot or the mark of when the change takes
    // effect, then the warning mark or the invalid mark.
    struct RowMarks
    {
        bool pending = false;
        model::Applies applies = model::Applies::Unknown;
        controls::Mark mark = controls::Mark::None;
    };

    void drawLabel(const ImVec2& pos, float right, std::string_view text, std::span<const model::TextRun> runs, const RowMarks& marks = {},
        const model::SearchQuery* query = nullptr);
    bool isShown(const model::Item& item, model::StoreReader* store);
    std::string_view warningOf(const std::vector<model::Warning>& warnings, model::StoreReader* store);
    std::string readOnlyHelp(std::string reason, const std::string& help);
}
