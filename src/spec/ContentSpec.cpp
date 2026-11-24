#include "ContentSpec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>

#include "ActionSpec.h"
#include "ActivationSphereSpec.h"
#include "ConditionSpec.h"
#include "SettingSpec.h"
#include "common/CommonUtils.h"
#include "model/OptionSets.h"
#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    using namespace spec;

    // the most lines a spacer is high, whatever its config spec says, as in an MCM config
    constexpr float MAX_SPACER_LINES = 20.0f;

    // the most characters of a row's name in a problem: a text row is a paragraph
    constexpr std::size_t MAX_ROW_NAME = 48;

    // The fields each row of a `content` can have, by its type, as the schema has them (docs/design/schema.md). Any
    // other is reported as one the menu does not know. A field the schema has and the menu does not read yet is known.
    constexpr std::array<std::string_view, 13>
        SECTION_FIELDS{ "type", "id", "text", "help", "content", "isCollapsible", "collapsed", "maxOn", "defaultFile", "applies", "omitDefault", "visibleWhen", "enabledWhen" };
    constexpr std::array<std::string_view, 4> TEXT_ROW_FIELDS{ "type", "text", "html", "visibleWhen" };
    constexpr std::array<std::string_view, 3> SPACER_FIELDS{ "type", "numLines", "visibleWhen" };
    constexpr std::array<std::string_view, 5> IMAGE_FIELDS{ "type", "texture", "width", "height", "visibleWhen" };

    /**
     * The `content` array of a page, a section or the config spec's root. None where it has none, or one that is no
     * array.
     */
    const Json* findContent(const Json& container)
    {
        const auto content = container.find("content");
        return content != container.end() && content->is_array() ? &*content : nullptr;
    }

    /**
     * The name of a row as a problem names it: its text as the player sees it, on one line with no tags and
     * shortened, or its type for a row with no text, as a spacer.
     */
    std::string rowName(const Json& row, const model::Translations& translations)
    {
        auto text = common::trim(model::removeMarkup(getShownText(row, "text", translations)));
        if (text.empty()) {
            text = getText(row, "type");
            return text.empty() ? "A row"s : text;
        }
        if (text.size() > MAX_ROW_NAME) {
            text = common::trim(text.substr(0, MAX_ROW_NAME - 3)) + "...";
        }
        return text;
    }

    /**
     * A text row: its text in lines, and with the tags of an MCM config read where its `html` is true.
     */
    model::Text loadText(const Json& row, ConfigSpecReading& reading)
    {
        const auto text = readShownText(row, "text", reading);
        return model::Text{ .lines = getBool(row, "html") ? model::parseMarkup(text) : model::plainLines(text), .visibleWhen = {} };
    }

    /**
     * A spacer: as high as its `numLines`, from 1 to 20, and one line without it.
     */
    model::Spacer loadSpacer(const Json& row)
    {
        const auto lines = std::clamp(getNumber(row, "numLines", 1.0f), 1.0f, MAX_SPACER_LINES);
        return model::Spacer{ .lines = static_cast<int>(lines), .visibleWhen = {} };
    }

    /**
     * An image: its `texture`, a path under `Data/Textures`, and its `width` and `height` in the panel's pixels,
     * of which a side that is not more than 0 is none. One with no texture shows that an image is there, and
     * is reported.
     */
    model::Image loadImage(const Json& row, ConfigSpecReading& reading)
    {
        auto texture = getText(row, "texture");
        if (texture.empty()) {
            reading.report(model::Severity::Error, "It names no texture: a box is shown in its place.");
        }
        return model::Image{ .pixels = {},
            .texture = std::move(texture),
            .width = (std::max)(getNumber(row, "width", 0.0f), 0.0f),
            .height = (std::max)(getNumber(row, "height", 0.0f), 0.0f),
            .visibleWhen = {} };
    }

    /**
     * A row of a `content` as the rows are read: the section a sphere was made into, and any other as it is.
     */
    const Json& readRow(const Json& row, const ConfigSpecReading& reading)
    {
        const auto sphere = reading.spheres.find(&row);
        return sphere != reading.spheres.end() ? sphere->second : row;
    }

    /**
     * The reading at a row of a `content`, by its key and its place: a row the loader made says where it is itself.
     */
    Into intoRow(ConfigSpecReading& reading, const Json& row, const std::string_view key, const std::size_t place)
    {
        if (const auto made = row.find(MADE_ROW_PATH); made != row.end()) {
            return Into(reading, made->get_ref<const std::string&>());
        }
        return Into(reading, key, place);
    }

    /**
     * A section's `maxOn`: a whole number of 1 or more, and 0 for none. Any other is an error, and the section has
     * no limit.
     */
    int loadMaxOn(const Json& row, ConfigSpecReading& reading)
    {
        const auto found = row.find("maxOn");
        if (found == row.end()) {
            return 0;
        }
        if (found->is_number_integer() && found->get<std::int64_t>() >= 1) {
            return static_cast<int>((std::min)(found->get<std::int64_t>(), static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
        }
        const Into into(reading, "maxOn");
        reading.report(model::Severity::Error, std::format("Its maxOn {} is not a whole number of 1 or more: it has no limit.", found->dump()));
        return 0;
    }

    /**
     * How many switchers a section's items have, in its nested sections too, and how many places their defaults take
     * on: each by its `countsAs`.
     */
    void countSwitchers(const std::vector<model::Item>& items, int& switchers, int& defaultsOn)
    {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                countSwitchers(group->items, switchers, defaultsOn);
            } else if (const auto* setting = std::get_if<model::Setting>(&item); setting && setting->type == model::SettingType::Switcher) {
                ++switchers;
                if (const auto* on = setting->defaultValue ? std::get_if<bool>(&*setting->defaultValue) : nullptr; on && *on) {
                    defaultsOn += setting->countsAs;
                }
            }
        }
    }

    /**
     * A section: a group of the items of its own `content`, to which it passes its `defaultFile` and its `applies`.
     * It folds with `isCollapsible`, and starts folded with `collapsed`: one that does not fold could not be
     * opened, so its `collapsed` is left out, and it is reported.
     * A `maxOn` limits the places its switchers take while they are on. One with no switchers has nothing to limit,
     * and one whose defaults take more places than it has could never be Reset to them: both are errors.
     */
    model::Group loadSection(const Json& row, const Inherited& above, ConfigSpecReading& reading)
    {
        checkFields(row, SECTION_FIELDS, reading);
        const bool collapsible = getBool(row, "isCollapsible");
        if (getBool(row, "collapsed") && !collapsible) {
            reading.report(model::Severity::Warning, "It is collapsed and does not fold: it is open.");
        }
        auto text = readShownText(row, "text", reading);
        auto help = readShownText(row, "help", reading);
        const auto maxOn = loadMaxOn(row, reading);
        auto own = inherit(row, above, reading);
        own.underLimit = own.underLimit || maxOn > 0;
        model::Group group{ .text = std::move(text),
            .help = std::move(help),
            .items = loadItems(row, own, reading),
            .visibleWhen = {},
            .collapsible = collapsible,
            .collapsed = collapsible && getBool(row, "collapsed"),
            .maxOn = maxOn };
        if (maxOn > 0) {
            int switchers = 0;
            int defaultsOn = 0;
            countSwitchers(group.items, switchers, defaultsOn);
            const Into into(reading, "maxOn");
            if (switchers == 0) {
                reading.report(model::Severity::Error, "It has a maxOn and no switchers: it has nothing to limit.");
            } else if (defaultsOn > maxOn) {
                reading.report(model::Severity::Error, std::format("Its switchers' defaults take {} places of its maxOn {}: Reset leaves it over its limit.", defaultsOn, maxOn));
            }
        }
        return group;
    }
}

namespace mods_config::spec
{
    /**
     * The items of the `content` of a page, a section or the config spec's root (`loadRows`). None for one with no
     * content.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading)
    {
        const auto* content = findContent(container);
        return content ? loadRows(*content, "content", above, reading) : std::vector<model::Item>{};
    }

    /**
     * The items of the rows of a `content`, by their `type`, one for each row, each read at `key` and its place: `content`
     * in spec.json, and none in the content a mod's client gives. An activation sphere is a section the loader makes of
     * the keys of its own. Any other row the menu does not have is a placeholder with its type, and a type the schema
     * does not have is reported. A row's fields that its type does not have are reported.
     */
    std::vector<model::Item> loadRows(const Json& rows, const std::string_view key, const Inherited& above, ConfigSpecReading& reading)
    {
        std::vector<model::Item> items;
        for (std::size_t place = 0; place < rows.size(); ++place) {
            const auto& row = rows[place];
            const auto into = intoRow(reading, row, key, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const auto type = getText(row, "type");
            if (type == "section") {
                items.emplace_back(loadSection(row, above, reading));
                continue;
            }
            if (type == "cf.activationSphere") {
                const auto& section = reading.spheres.emplace(&row, sphereSection(row, above, reading)).first->second;
                auto group = loadSection(section, above, reading);
                group.role = model::GroupRole::ActivationSphere;
                items.emplace_back(std::move(group));
                continue;
            }
            if (type == "text") {
                checkFields(row, TEXT_ROW_FIELDS, reading);
                items.emplace_back(loadText(row, reading));
                continue;
            }
            if (type == "spacer") {
                checkFields(row, SPACER_FIELDS, reading);
                items.emplace_back(loadSpacer(row));
                continue;
            }
            if (type == "image") {
                checkFields(row, IMAGE_FIELDS, reading);
                items.emplace_back(loadImage(row, reading));
                continue;
            }
            if (type == "button") {
                items.emplace_back(loadButton(row, reading));
                continue;
            }
            if (const auto settingType = findSettingType(type)) {
                items.emplace_back(loadSetting(row, *settingType, above, reading));
                continue;
            }
            reading.report(model::Severity::Warning,
                type.empty() ? "It has no type: it is shown as a row the menu does not have."s
                             : std::format("The menu has no row of the type \"{}\": it is shown as one it does not have.", type));
            auto text = getLabel(row, reading);
            auto help = readShownText(row, "help", reading);
            items.emplace_back(
                model::Placeholder{ .text = std::move(text), .textRuns = getLabelRuns(row, reading.translations), .help = std::move(help), .type = type, .visibleWhen = {} });
        }
        return items;
    }

    /**
     * Gives the items of the `content` of a page, a section or the config spec's root their conditions
     * (`loadRowsConditions`).
     */
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading)
    {
        if (const auto* content = findContent(container)) {
            loadRowsConditions(*content, "content", items, above, reading);
        }
    }

    /**
     * Gives the items of the rows of a `content`, read at `key` as `loadRows` reads them, their conditions: a row's
     * `visibleWhen` and `enabledWhen`, each joined with those of the sections it is in, so a row needs every condition
     * above it, and the warnings of a setting and a button. A section's heading is shown by its `visibleWhen` too.
     * The items are the rows, one for each, in their order.
     */
    void loadRowsConditions(const Json& rows, const std::string_view key, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading)
    {
        for (std::size_t place = 0; place < rows.size() && place < items.size(); ++place) {
            const auto& row = readRow(rows[place], reading);
            auto& item = items[place];
            const auto into = intoRow(reading, row, key, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const RowConditions own{ .visibleWhen = joinConditions(above.visibleWhen, loadRowCondition(row, "visibleWhen", reading)),
                .enabledWhen = joinConditions(above.enabledWhen, loadRowCondition(row, "enabledWhen", reading)) };
            std::visit(
                [&own](auto& loaded) {
                    loaded.visibleWhen = own.visibleWhen;
                },
                item);
            if (auto* setting = std::get_if<model::Setting>(&item)) {
                setting->enabledWhen = own.enabledWhen;
                setting->warnings = loadWarnings(row, reading);
                if (model::setsOthers(*setting)) {
                    loadOptionSets(row, *setting, reading);
                }
                if (std::holds_alternative<model::ColorPartsStorage>(setting->storage)) {
                    loadColorParts(row, *setting, reading);
                }
            } else if (auto* button = std::get_if<model::Button>(&item)) {
                button->enabledWhen = own.enabledWhen;
                button->warnings = loadWarnings(row, reading);
            } else if (auto* group = std::get_if<model::Group>(&item)) {
                loadConditions(row, group->items, own, reading);
            }
        }
    }

    /**
     * Hides the rows of the sliders colors are kept in, of items and of the sections in them, by their ids in lower case
     * (`ConfigSpecReading::colorParts`): the color's row edits them. Their conditions are read already, and so a slider
     * on any page, before its color or after it, is hidden.
     */
    void hideColorParts(std::vector<model::Item>& items, const std::set<std::string, std::less<>>& parts)
    {
        for (auto& item : items) {
            if (auto* group = std::get_if<model::Group>(&item)) {
                hideColorParts(group->items, parts);
            } else if (auto* setting = std::get_if<model::Setting>(&item); setting && parts.contains(common::str_tolower(setting->id))) {
                setting->visibleWhen = model::Condition{ .kind = model::Condition::Kind::Any, .setting = {}, .conditions = {} };
            }
        }
    }

    /**
     * Gives the sections, the activation spheres and the buttons of the `content` of a page, a section or the config
     * spec's root their `id` (`loadRowsIds`).
     */
    void loadItemIds(const Json& container, std::vector<model::Item>& items, ConfigSpecReading& reading)
    {
        if (const auto* content = findContent(container)) {
            loadRowsIds(*content, "content", items, reading);
        }
    }

    /**
     * Gives the sections, the activation spheres and the buttons of the rows of a `content`, read at `key` as `loadRows`
     * reads them, their `id`, which their mod's client is told while the ray is on them: once every setting is read, as
     * an id is unique among the settings' too. One a setting has, or a section or a button before it, is an error, and
     * it is left out: the setting keeps its own, as conditions name it.
     */
    void loadRowsIds(const Json& rows, const std::string_view key, std::vector<model::Item>& items, ConfigSpecReading& reading)
    {
        for (std::size_t place = 0; place < rows.size() && place < items.size(); ++place) {
            const auto& row = readRow(rows[place], reading);
            auto& item = items[place];
            auto* group = std::get_if<model::Group>(&item);
            auto* button = std::get_if<model::Button>(&item);
            if (!group && !button) {
                continue;
            }
            const auto into = intoRow(reading, row, key, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            if (auto id = getText(row, "id"); !id.empty()) {
                const auto lower = common::str_tolower(id);
                if (const auto setting = reading.settings.find(lower); setting != reading.settings.end()) {
                    reading.report(model::Severity::Error,
                        std::format("The setting \"{}\" has its id: the setting keeps it, and this tells its mod's client nothing. Give it an id of its own.", setting->second.id));
                } else if (!reading.itemIds.insert(lower).second) {
                    reading.report(model::Severity::Error,
                        std::format("A section or a button before it has the id \"{}\": this tells its mod's client nothing. Give it an id of its own.", id));
                } else {
                    (group ? group->id : button->id) = std::move(id);
                }
            }
            if (group) {
                loadItemIds(row, group->items, reading);
            }
        }
    }
}
