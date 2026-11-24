#include "StoreSession.h"

namespace mods_config::ui
{
    /**
     * Shows a mod of a store, or the menu's front page with no mod and no store. Another mod starts its session in its store, after
     * the store before ends its own; the same mod is told another page is shown. Its rows then read their texts, and
     * only then is `resolvePage` asked which page shows, as its conditions ask the open store. A page whose rows the
     * mod's client gives is asked for them, and the store is told the page, and that no row is pointed when one was.
     */
    void StoreSession::show(model::Store* store, model::Mod* mod, const std::function<model::Page*()>& resolvePage)
    {
        const bool sameMod = store == _store && (mod ? mod->name : std::string()) == _modName;
        if (!sameMod) {
            // a store ends the session it had as it opens another, and tells its client no row then
            if (_store && _store != store) {
                _store->close();
            }
            _store = store;
            _modName = mod ? mod->name : std::string();
            _pointed = nullptr;
            if (_store) {
                _store->open(*mod);
            }
        } else if (_store) {
            _store->pageShown();
        }
        if (_store) {
            _store->readTexts(*mod);
        }
        auto* const page = resolvePage();
        if (_store && page) {
            _store->readPageContent(*mod, *page);
        }
        if (_store) {
            _store->showPage(page);
        }
        point(nullptr);
    }

    /**
     * Asks the mod's client again for the rows of the page that is shown, while it is, as `show` asks as it is shown.
     * Returns whether it asked: the store is then told that no row is pointed, as the old rows are gone.
     */
    bool StoreSession::readPageContent(model::Mod& mod, model::Page& page)
    {
        if (!_store || !_store->readPageContent(mod, page)) {
            return false;
        }
        _pointed = nullptr;
        _store->showPointed(nullptr);
        return true;
    }

    /**
     * The item of the shown mod whose row the ray is on, which its store is told as it changes: none for no row.
     */
    void StoreSession::point(const model::Item* item)
    {
        if (item == _pointed) {
            return;
        }
        _pointed = item;
        if (_store) {
            _store->showPointed(item);
        }
    }

    /**
     * A frame of the panel starts, which the store of the shown mod is told.
     */
    void StoreSession::startFrame() const
    {
        if (_store) {
            _store->startFrame();
        }
    }

    /**
     * Ends the session of the shown mod, as the menu closes, its mods are replaced, or the shown mod is read again:
     * its store ends it, and tells its client no row and no page. Nothing with no session.
     */
    void StoreSession::end()
    {
        if (_store) {
            _store->close();
        }
        _store = nullptr;
        _modName.clear();
        _pointed = nullptr;
    }
}
