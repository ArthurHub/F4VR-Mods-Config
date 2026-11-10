#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "HelpLine.h"
#include "model/ConfigDefinition.h"
#include "model/Search.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * The buttons of the page that is shown, as the design system's ButtonRow has them: how one is drawn, the
     * confirmation one asks before it runs, and the result of its last run. A result that is done has the info mark
     * after the button's text until the ray leaves the row after being on it, and one that failed has the invalid
     * mark: the help line has either until the button runs again or the page is left.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class Buttons
    {
    public:
        std::optional<PointedRow> draw(const model::Button& button, model::Store* store, bool& pressActed, const model::SearchQuery* query = nullptr);
        bool drawDialog(model::Store* store);
        bool cancelConfirmation();
        void reset();

    private:
        /**
         * The result of a button's last run that gave one: a message, or a failure.
         */
        struct Result
        {
            bool failed = false;
            std::string message;

            // the info mark of a result that is done: shown until the ray leaves the row after being on it
            bool unread = true;
            bool seen = false;
        };

        bool run(const model::Button& button, model::Store& store);

        std::unordered_map<const model::Button*, Result> _results;

        // the button whose confirmation is shown, and none
        const model::Button* _confirming = nullptr;
    };
}
