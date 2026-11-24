#include "ModList.h"

#include <algorithm>
#include <format>
#include <functional>
#include <numeric>

#include "common/CommonUtils.h"

namespace
{
    // how the error of a live copy that cannot be read starts, which tells it from the config spec's other problems
    constexpr std::string_view LIVE_COPY_NOT_READ = "The live copy cannot be read";
}

namespace mods_config::ui
{
    /**
     * The `modName` of the menu's own config spec, matched in any case, whose Categories page has the rows whose
     * pending names the rows' options offer. Told once, before the mods.
     */
    void ModList::setOwnMod(std::string modName)
    {
        _ownMod = std::move(modName);
    }

    /**
     * The shipped table of the categories, which gives a mod with no category of its own one. Set once at startup.
     */
    void ModList::setCategoryTable(model::CategoryTable table)
    {
        _categoryTable = std::move(table);
        group();
    }

    /**
     * The categories the player gave mods, as their file has them: at first, and after a Save wrote it.
     */
    void ModList::setCategoryChoices(model::CategoryChoices choices)
    {
        _categoryChoices = std::move(choices);
        group();
    }

    /**
     * From how many mods the tree is grouped by category: 0 groups any list.
     */
    void ModList::setCategoriesMinMods(const int minMods)
    {
        if (_categoriesMinMods != minMods) {
            _categoriesMinMods = minMods;
            group();
        }
    }

    /**
     * The options of a row of the Categories page: the names in use, with the player's choices from their file and the
     * ones on the own mod's page that wait for a Save in `store`, so a name typed in a row is in the others' lists.
     */
    std::vector<model::Option> ModList::categoryOptions(model::StoreReader& store) const
    {
        const auto own = std::ranges::find_if(_mods, std::bind_front(&ModList::isOwnMod, this));
        const auto choices = own != _mods.end() ? model::withPendingChoices(_categoryChoices, *own, store) : _categoryChoices;
        return model::categoryOptions(_mods, _categoryTable, choices);
    }

    /**
     * The rows of the Categories page of the own mod, which its client gives as the page is shown: one for each mod
     * the menu lists.
     */
    std::string ModList::categoryPageContent() const
    {
        return model::categoryPageContent(_mods, _categoryTable);
    }

    /**
     * Tells that the live copy of the config spec of a folder cannot be read, with the reason: its mod keeps what it
     * read last, with its unsaved changes, and is read from the copy, where the report says it is. The reason is an
     * error before its problems, in place of the one of a read before, and the last read of its live copy says that
     * it failed, and when, as `10:42:05`.
     */
    void ModList::liveCopyNotRead(const std::string_view folder, const std::string_view liveCopy, const std::string_view time, const std::string_view reason)
    {
        const auto place = placeOf(folder);
        if (!place) {
            return;
        }
        auto& mod = _mods[*place];
        mod.liveCopy = liveCopy;
        std::erase_if(mod.problems, [](const model::Problem& problem) {
            return problem.message.starts_with(LIVE_COPY_NOT_READ);
        });
        mod.problems.insert(mod.problems.begin(),
            model::Problem{ .severity = model::Severity::Error,
                .row = "spec.json",
                .message = std::format("{}: {}. The menu shows the copy it read last.", LIVE_COPY_NOT_READ, reason),
                .where = {},
                .path = {},
                .place = {} });
        _liveCopyReads[std::string(folder)] =
            LiveCopyRead{ .result = std::format("Read at {}: the live copy cannot be read. The menu shows the copy it read last.", time), .failed = true };
    }

    /**
     * Tells whether the installed spec.json of the config spec of a folder is newer than its live copy, as the copy
     * was just read, which the last read keeps until the next: Overwrite warns of it.
     */
    void ModList::setLiveCopyStale(const std::string_view folder, const bool stale)
    {
        if (auto* const read = changeableRead(folder)) {
            read->stale = stale;
        }
    }

    /**
     * The place of the config spec of a folder among the mods, and none for no folder or one that is not listed.
     */
    std::optional<std::size_t> ModList::placeOf(const std::string_view folder) const
    {
        const auto found = std::ranges::find_if(_mods, [folder](const model::Mod& each) {
            return each.source == model::Source::ConfigSpec && each.folder == folder;
        });
        if (folder.empty() || found == _mods.end()) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(found - _mods.begin());
    }

    /**
     * The mods the menu lists, from every source they come from, kept in order of their display names.
     */
    void ModList::setMods(std::vector<model::Mod> mods)
    {
        std::ranges::sort(mods, common::CaseInsensitiveComparator(), &model::Mod::displayName);
        _mods = std::move(mods);
        group();
    }

    /**
     * Puts a config spec's mod that was read again in place of the one of its folder, sorts the mods again, as its
     * display name can be another now, and groups them again. The last read of its live copy says when it was
     * read, as `10:42:05`, and how many unsaved changes it dropped. Returns the new place of each old place, and none
     * for a mod of no folder or one that is not listed, which is not put in.
     */
    std::optional<std::vector<std::size_t>> ModList::replace(model::Mod mod, const std::size_t dropped, const std::string_view time)
    {
        const auto found = placeOf(mod.folder);
        if (!found) {
            return std::nullopt;
        }
        const auto folder = mod.folder;
        _mods[*found] = std::move(mod);

        std::vector<std::size_t> order(_mods.size());
        std::iota(order.begin(), order.end(), std::size_t{ 0 });
        std::ranges::stable_sort(order, common::CaseInsensitiveComparator(), [this](const std::size_t place) -> const std::string& {
            return _mods[place].displayName;
        });
        std::vector<std::size_t> placeOfOld(_mods.size());
        std::vector<model::Mod> sorted;
        sorted.reserve(_mods.size());
        for (std::size_t place = 0; place < order.size(); ++place) {
            placeOfOld[order[place]] = place;
            sorted.push_back(std::move(_mods[order[place]]));
        }
        _mods = std::move(sorted);

        group();
        _liveCopyReads[folder] =
            LiveCopyRead{ .result = dropped == 0 ? std::format("Reloaded at {}.", time)
                                                 : std::format("Reloaded at {}: {} unsaved change{} dropped.", time, dropped, dropped == 1 ? " was" : "s were"),
                .failed = false };
        return placeOfOld;
    }

    bool ModList::isOwnMod(const model::Mod& mod) const
    {
        return !_ownMod.empty() && mod.source == model::Source::ConfigSpec && model::categoryKey(mod.name) == model::categoryKey(_ownMod);
    }

    /**
     * Groups the mods by their categories again: none for a list too short to be grouped, whose tree is flat.
     */
    void ModList::group()
    {
        _groups.clear();
        if (_categoriesMinMods && !_mods.empty() && model::isGrouped(_mods.size(), *_categoriesMinMods)) {
            _groups = model::groupByCategory(_mods, _categoryTable, _categoryChoices);
        }
    }
}
