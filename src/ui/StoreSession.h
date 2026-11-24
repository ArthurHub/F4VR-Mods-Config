#pragma once

#include <functional>
#include <string>

#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * The editing session of the mod the panel shows, as its store is told of it: which mod and page are shown and
     * which row is pointed. The panel says what is shown, what is pointed, and that a frame starts; this tells the
     * store once and in order for each real change, so no path of the panel orders the store's calls itself.
     */
    class StoreSession
    {
    public:
        void show(model::Store* store, model::Mod* mod, const std::function<model::Page*()>& resolvePage);
        bool readPageContent(model::Mod& mod, model::Page& page);
        void point(const model::Item* item);
        void startFrame() const;
        void end();

        // the item whose row the ray is on, which the store was told last: none for no row
        const model::Item* pointed() const
        {
            return _pointed;
        }

    private:
        // the store and the mod whose session it is, by the mod's name, which stays as the panel sorts its mods
        // again: none on the menu's front page
        model::Store* _store = nullptr;
        std::string _modName;

        const model::Item* _pointed = nullptr;
    };
}
