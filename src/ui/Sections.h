#pragma once

#include <string>
#include <unordered_map>

#include <imgui.h>

#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * What the heading row of a section that folds tells of the pointer in the frame that is drawn.
     */
    struct SectionRow
    {
        // false for a row that is scrolled out of the page: nothing of it is drawn
        bool shown = false;

        // the ray is on the row
        bool pointed = false;

        // the trigger is held since a pull on the row, and was let go on it in this frame
        bool held = false;
        bool pressed = false;
    };

    /**
     * The sections that fold of the page that is shown: how the heading of one is drawn, and whether each is
     * folded. A press on its heading folds or opens it, and it starts as its `collapsed` says each time its
     * page is shown.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Sections
    {
    public:
        bool isFolded(const model::Group& group) const;
        bool drawHeading(const model::Group& group, model::Store* store, const ImVec2& pos, float width, const SectionRow& row, std::string& help);
        void reset();

    private:
        // the sections the player folded or opened, with whether each is folded now: any other is as its
        // `collapsed` says
        std::unordered_map<const model::Group*, bool> _folded;
    };
}
