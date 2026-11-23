#include "ConfigSpecLoader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "ActionSpec.h"
#include "ActivationSphereSpec.h"
#include "ChoiceOptionsSpec.h"
#include "ConditionSpec.h"
#include "ConfigSpecReading.h"
#include "FrameworkTypeSpec.h"
#include "RequirementSpec.h"
#include "SettingSpec.h"
#include "StorageSpec.h"
#include "common/CommonUtils.h"
#include "model/Categories.h"
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

    // The fields each object of a config spec can have, as the schema has them (docs/design/schema.md). Any other is
    // reported as one the menu does not know. A field the schema has and the menu does not read yet is known.
    // the config spec's root:
    constexpr std::array<std::string_view, 16> ROOT_FIELDS{ "minSchemaVersion",
        "modName",
        "displayName",
        "help",
        "category",
        "specVersion",
        "pluginRequirements",
        "f4sePluginRequirements",
        "files",
        "defaultFile",
        "applies",
        "omitDefault",
        "sharedLists",
        "sharedPresetLists",
        "content",
        "pages" };
    // a page:
    constexpr std::array<std::string_view, 10>
        PAGE_FIELDS{ "pageDisplayName", "help", "id", "content", "pages", "defaultFile", "applies", "omitDefault", "visibleWhen", "enabledWhen" };
    // a row, by its type:
    constexpr std::array<std::string_view, 12>
        SECTION_FIELDS{ "type", "id", "text", "help", "content", "isCollapsible", "collapsed", "defaultFile", "applies", "omitDefault", "visibleWhen", "enabledWhen" };
    constexpr std::array<std::string_view, 4> TEXT_ROW_FIELDS{ "type", "text", "html", "visibleWhen" };
    constexpr std::array<std::string_view, 3> SPACER_FIELDS{ "type", "numLines", "visibleWhen" };
    constexpr std::array<std::string_view, 5> IMAGE_FIELDS{ "type", "texture", "width", "height", "visibleWhen" };

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
     * The reading at a row of a `content`, by its place: a row the loader made says where it is itself.
     */
    Into intoRow(ConfigSpecReading& reading, const Json& row, const std::size_t place)
    {
        if (const auto made = row.find(MADE_ROW_PATH); made != row.end()) {
            return Into(reading, made->get_ref<const std::string&>());
        }
        return Into(reading, "content", place);
    }

    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading);

    /**
     * A section: a group of the items of its own `content`, to which it passes its `defaultFile` and its `applies`.
     * It folds with `isCollapsible`, and starts folded with `collapsed`: one that does not fold could not be
     * opened, so its `collapsed` is left out, and it is reported.
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
        return model::Group{ .text = std::move(text),
            .help = std::move(help),
            .items = loadItems(row, inherit(row, above, reading), reading),
            .visibleWhen = {},
            .collapsible = collapsible,
            .collapsed = collapsible && getBool(row, "collapsed") };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`, one for each row. An activation sphere is a
     * section the loader makes of the keys of its own. Any other row the menu does not have is a placeholder with its
     * type, and a type the schema does not have is reported. A row's fields that its type does not have are reported.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading)
    {
        std::vector<model::Item> items;
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return items;
        }
        for (std::size_t place = 0; place < content->size(); ++place) {
            const auto& row = (*content)[place];
            const auto into = intoRow(reading, row, place);
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
     * A page of a config spec, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do. A problem in it is on it: the caller reads it `Into` its place, with its place among the pages,
     * which takes the page back off.
     */
    model::Page loadPage(const Json& page, const Inherited& above, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(getShownText(page, "pageDisplayName", reading.translations)));
        checkFields(page, PAGE_FIELDS, reading);
        model::Page loaded{ .name = readShownText(page, "pageDisplayName", reading),
            .help = readShownText(page, "help", reading),
            .items = {},
            .pages = {},
            .id = getText(page, "id") };
        const auto own = inherit(page, above, reading);
        loaded.items = loadItems(page, own, reading);
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loaded.pages.push_back(loadPage((*pages)[place], own, reading));
            }
        }
        return loaded;
    }

    /**
     * What a row is shown and enabled by: its own conditions, and those of the sections it is in.
     */
    struct RowConditions
    {
        std::optional<model::Condition> visibleWhen;
        std::optional<model::Condition> enabledWhen;
    };

    /**
     * Gives the items of the `content` of a page or a section their conditions: a row's `visibleWhen` and
     * `enabledWhen`, each joined with those of the sections it is in, so a row needs every condition above
     * it, and the warnings of a setting and a button. A section's heading is shown by its `visibleWhen` too.
     * The items are the rows of the content, one
     * for each, in their order, as `loadItems` reads them.
     */
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = readRow((*content)[place], reading);
            auto& item = items[place];
            const auto into = intoRow(reading, row, place);
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
            } else if (auto* button = std::get_if<model::Button>(&item)) {
                button->enabledWhen = own.enabledWhen;
                button->warnings = loadWarnings(row, reading);
            } else if (auto* group = std::get_if<model::Group>(&item)) {
                loadConditions(row, group->items, own, reading);
            }
        }
    }

    /**
     * Gives the sections, the activation spheres and the buttons of the `content` of a page or a section their `id`,
     * which their mod's client is told while the ray is on them: once every setting is read, as an id is unique among
     * the settings' too. One a setting has, or a section or a button before it, is an error, and it is left out: the
     * setting keeps its own, as conditions name it.
     */
    void loadItemIds(const Json& container, std::vector<model::Item>& items, ConfigSpecReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = readRow((*content)[place], reading);
            auto& item = items[place];
            auto* group = std::get_if<model::Group>(&item);
            auto* button = std::get_if<model::Button>(&item);
            if (!group && !button) {
                continue;
            }
            const auto into = intoRow(reading, row, place);
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

    /**
     * Gives the sections, the activation spheres and the buttons of a page and of its child pages their `id`.
     */
    void loadPageItemIds(const Json& page, model::Page& loaded, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loadItemIds(page, loaded.items, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageItemIds((*pages)[place], loaded.pages[place], reading);
        }
    }

    /**
     * Gives a page, its items and its child pages their conditions, including those of its parent pages.
     */
    void loadPageConditions(const Json& page, model::Page& loaded, ConfigSpecReading& reading, const RowConditions& above = {})
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loaded.visibleWhen = joinConditions(above.visibleWhen, loadRowCondition(page, "visibleWhen", reading));
        loaded.enabledWhen = joinConditions(above.enabledWhen, loadRowCondition(page, "enabledWhen", reading));
        const RowConditions own{ .visibleWhen = loaded.visibleWhen, .enabledWhen = loaded.enabledWhen };
        loadConditions(page, loaded.items, own, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageConditions((*pages)[place], loaded.pages[place], reading, own);
        }
    }

    /**
     * Runs `read` on each page of a config spec's `pages`, with the page and its place among them, while the reading is
     * at the page. Nothing for a config spec with no pages.
     */
    template <typename Read>
    void forEachPage(const Json& configSpec, ConfigSpecReading& reading, Read read)
    {
        const auto pages = configSpec.find("pages");
        if (pages == configSpec.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            read((*pages)[place], place);
        }
    }

    /**
     * Reads what can name a setting of any page, once every setting of the config spec is read: the conditions and the
     * warnings of the rows of the front page and of every page, and then the ids of their sections, spheres and buttons,
     * which no setting's id can be.
     */
    void loadConditionsAndIds(const Json& configSpec, model::Mod& mod, ConfigSpecReading& reading)
    {
        loadConditions(configSpec, mod.frontPage.items, {}, reading);
        forEachPage(configSpec, reading, [&](const Json& page, const std::size_t place) {
            loadPageConditions(page, mod.pages[place], reading);
        });
        loadItemIds(configSpec, mod.frontPage.items, reading);
        forEachPage(configSpec, reading, [&](const Json& page, const std::size_t place) {
            loadPageItemIds(page, mod.pages[place], reading);
        });
    }

    /**
     * A path as UTF-8 text for the log. A path's own string() throws on a name the system's code page lacks.
     */
    std::string toLogText(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return { text.begin(), text.end() };
    }

    /**
     * Whether a problem is before another in spec.json, by their paths: a place in a list is compared as a
     * number, so `content[2]` is before `content[10]`, and a path is before the paths in it. The keys of an
     * object have no order in the file the reader keeps, so they are in the order of their names.
     */
    bool isBefore(const model::Problem& problem, const model::Problem& other)
    {
        // the parts of a path: the names, and the places, which are numbers
        const auto parts = [](const std::string& path) {
            std::vector<std::variant<std::size_t, std::string>> parts;
            std::size_t at = 0;
            while (at < path.size()) {
                if (path[at] == '.') {
                    ++at;
                } else if (path[at] == '[') {
                    const auto end = path.find(']', at);
                    std::size_t place = 0;
                    std::from_chars(path.data() + at + 1, path.data() + end, place);
                    parts.emplace_back(place);
                    at = end + 1;
                } else {
                    const auto end = path.find_first_of(".[", at);
                    parts.emplace_back(path.substr(at, end - at));
                    at = end == std::string::npos ? path.size() : end;
                }
            }
            return parts;
        };
        return parts(problem.path) < parts(other.path);
    }

    /**
     * A count of a thing as the log says it: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * Logs each problem of a config spec, with the config spec's name first, where it is after it, and how many
     * errors and warnings the config spec has.
     */
    void logProblems(const model::Mod& mod)
    {
        std::size_t errors = 0;
        for (const auto& problem : mod.problems) {
            const bool error = problem.severity == model::Severity::Error;
            errors += error ? 1 : 0;
            auto where = problem.where;
            if (!problem.path.empty()) {
                where += (where.empty() ? "" : ", ") + problem.path;
            }
            logger::warn("Config spec '{}': {} of {}: {}{}", mod.displayName, error ? "error" : "warning", problem.row, problem.message, where.empty() ? "" : " (" + where + ")");
        }
        logger::info("Config spec '{}' has {} and {}", mod.displayName, countOf(errors, "error"), countOf(mod.problems.size() - errors, "warning"));
    }

    /**
     * A mod that is not available, which has its names and the reason alone.
     */
    model::Mod notAvailableMod(std::string name, std::string displayName, std::string reason)
    {
        return model::Mod{ .name = std::move(name),
            .displayName = std::move(displayName),
            .frontPage = {},
            .pages = {},
            .notAvailable = std::move(reason),
            .help = {},
            .source = model::Source::ConfigSpec,
            .files = {} };
    }
}

namespace mods_config::spec
{
    /**
     * Reads the text of a config spec's `spec.json` into the model: the mod and its help, its files, its front
     * page and its pages, which nest, with their help, and their sections, which nest and fold, with their
     * help, settings, buttons, texts, spacers and images. Every
     * text the player sees is in the game's language. The config spec's root has to say its `minSchemaVersion`. A
     * setting has the file its value is kept in, and when its change takes effect, which the config spec's root has
     * to say. A choice has its options or a list of
     * the config spec's `sharedLists`. A row is shown and enabled by its conditions, which are read once every
     * setting is, as a condition can name a setting after it. A condition on a plugin is answered by the checks.
     * A mod whose config spec needs a newer schema than the menu reads, or whose required plugins are not all
     * loaded, is not available, and is its names and the reason alone: the rest of its config spec is not read.
     * The error says why a config spec cannot be read. Comments are skipped, as in an MCM config.
     * With the folders of its files, the files that are there are read, and a setting whose path its file does not
     * have is a warning.
     */
    std::expected<model::Mod, std::string> loadConfigSpec(const std::string_view text, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        Json configSpec;
        try {
            configSpec = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!configSpec.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(configSpec, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }
        const auto schemaVersion = findSchemaVersion(configSpec);
        if (!schemaVersion) {
            return std::unexpected("No minSchemaVersion of a whole number from 1"s);
        }
        const auto sharedLists = configSpec.find("sharedLists");
        const auto sharedPresetLists = configSpec.find("sharedPresetLists");
        ConfigSpecReading reading{ .translations = translations,
            .files = {},
            .modName = mod.name,
            .sharedLists = sharedLists != configSpec.end() && sharedLists->is_object() ? &*sharedLists : nullptr,
            .sharedPresetLists = sharedPresetLists != configSpec.end() && sharedPresetLists->is_object() ? &*sharedPresetLists : nullptr,
            .plugins = plugins,
            .folders = folders,
            .settings = {},
            .problems = {},
            .at = {} };
        mod.source = model::Source::ConfigSpec;
        mod.displayName = readShownText(configSpec, "displayName", reading);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        // a newer schema can say anything in another way, so nothing more of it is read
        auto reason = *schemaVersion > SCHEMA_VERSION ? std::format("its spec.json needs a newer Mods Config FO4 VR, one that reads schema version {}", *schemaVersion)
                                                      : findMissingRequirements(configSpec, plugins);
        mod.category = model::categoryName(getText(configSpec, "category"));
        if (!reason.empty()) {
            auto notAvailable = notAvailableMod(std::move(mod.name), std::move(mod.displayName), std::move(reason));
            notAvailable.category = std::move(mod.category);
            return notAvailable;
        }
        checkFields(configSpec, ROOT_FIELDS, reading);
        if (configSpec.contains("category") && mod.category.empty()) {
            const Into into(reading, "category");
            reading.report(model::Severity::Warning, "The category is not read: it is a plain name, as \"Gameplay\", and not a `$` token.");
        }
        mod.help = readShownText(configSpec, "help", reading);
        mod.f4sePlugins = requiredPlugins(configSpec, "f4sePluginRequirements");
        reading.files = loadFiles(configSpec, reading);
        reading.fileValues = readFiles(reading.files, folders);
        checkSharedLists(reading);
        checkSharedPresetLists(reading);

        // a setting that nothing says it of takes effect at a time the menu does not know
        const auto root = inherit(configSpec, Inherited{}, reading);
        mod.frontPage.items = loadItems(configSpec, root, reading);
        forEachPage(configSpec, reading, [&](const Json& page, std::size_t) {
            mod.pages.push_back(loadPage(page, root, reading));
        });
        loadConditionsAndIds(configSpec, mod, reading);
        mod.files = std::move(reading.files);
        // the conditions are read after every row, and their problems are put in their places
        std::ranges::stable_sort(reading.problems, isBefore);
        mod.problems = std::move(reading.problems);
        return mod;
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * Reads a config spec's `spec.json` from its file, as `loadConfigSpec` reads its text. The error says why it
     * cannot be read, also for a file that cannot be opened.
     */
    std::expected<model::Mod, std::string> loadConfigSpecFile(const std::filesystem::path& file, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return std::unexpected("The file cannot be opened"s);
        }
        std::ostringstream text;
        text << stream.rdbuf();
        return loadConfigSpec(text.str(), translations, plugins, folders);
    }

    /**
     * The mod of a config spec whose `spec.json` cannot be read: not available, named by its folder, with the
     * reason as its error.
     */
    model::Mod notLoadedConfigSpec(const std::string& folder, const std::string& reason)
    {
        auto mod = notAvailableMod(folder, folder, "its spec.json cannot be read");
        mod.problems.push_back({ .severity = model::Severity::Error, .row = SPEC_FILE, .message = std::format("It cannot be read: {}.", reason), .where = {}, .path = {} });
        mod.folder = folder;
        return mod;
    }

    /**
     * Logs why a config spec's mod is not available, each of its problems, with the config spec's name first, and
     * how many errors and warnings it has.
     */
    void logConfigSpec(const model::Mod& mod)
    {
        if (!mod.notAvailable.empty()) {
            logger::warn("Config spec '{}' is not available: {}", mod.name, mod.notAvailable);
        }
        logProblems(mod);
    }

    /**
     * The folders of the running game a config spec's files are read from: the game's Data folder, and
     * `Documents\My Games\Fallout4VR`.
     */
    FileFolders gameFileFolders()
    {
        return { .data = "Data", .myGames = common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)") };
    }

    /**
     * The checks of the running game: a game plugin it loaded, and an F4SE plugin F4SE loaded.
     */
    PluginChecks gamePluginChecks()
    {
        return { .isGamePluginLoaded = isGamePluginLoaded, .isF4sePluginLoaded = isF4sePluginLoaded };
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * first, and how many errors and warnings each config spec has. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigSpecs(const std::filesystem::path& configFolder, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::vector<model::Mod> mods;
        std::error_code error;
        for (std::filesystem::directory_iterator folder(configFolder, error), end; !error && folder != end; folder.increment(error)) {
            const auto file = folder->path() / SPEC_FILE;
            std::error_code noFile;
            if (!std::filesystem::is_regular_file(file, noFile)) {
                continue;
            }
            auto mod = loadConfigSpecFile(file, translations, plugins, folders);
            const auto folderName = toLogText(folder->path().filename());
            if (!mod) {
                mod = notLoadedConfigSpec(folderName, mod.error());
            }
            mod->folder = folderName;
            mods.push_back(std::move(*mod));
        }

        std::map<std::string, int> names;
        for (const auto& mod : mods) {
            ++names[common::str_tolower(mod.name)];
        }
        for (auto& mod : mods) {
            if (mod.notAvailable.empty() && names[common::str_tolower(mod.name)] > 1) {
                auto folder = std::move(mod.folder);
                mod = notAvailableMod(mod.name, mod.displayName, "another config spec has its modName");
                mod.folder = std::move(folder);
                mod.problems.push_back({ .severity = model::Severity::Error,
                    .row = SPEC_FILE,
                    .message = std::format("Another config spec has the modName \"{}\": neither is read.", mod.name),
                    .where = {},
                    .path = "modName" });
            }
            logConfigSpec(mod);
        }
        return mods;
    }

    /**
     * The config specs of the running game, from `Data\ModsConfigFO4\Config` in the game's folder, with their
     * conditions on a plugin answered by the plugins the game loaded.
     */
    std::vector<model::Mod> loadInstalledConfigSpecs(const model::Translations& translations)
    {
        auto mods = loadConfigSpecs(CONFIG_SPEC_FOLDER, translations, gamePluginChecks(), gameFileFolders());
        logger::info("Loaded {} config specs from '{}'", mods.size(), CONFIG_SPEC_FOLDER);
        return mods;
    }

    /**
     * Every mod the menu lists: the MCM mods and then the config specs. A mod that has both is shown by its config spec
     * alone, matched by its `modName` without regard to case, also one that is not available: its MCM menu is left out.
     */
    std::vector<model::Mod> withConfigSpecs(std::vector<model::Mod> mcmMods, std::vector<model::Mod> configSpecs)
    {
        std::set<std::string> names;
        for (const auto& mod : configSpecs) {
            names.insert(common::str_tolower(mod.name));
        }
        std::erase_if(mcmMods, [&names](const model::Mod& mod) {
            if (!names.contains(common::str_tolower(mod.name))) {
                return false;
            }
            logger::info("The MCM menu of '{}' is left out: its config spec is listed", mod.name);
            return true;
        });
        std::ranges::move(configSpecs, std::back_inserter(mcmMods));
        return mcmMods;
    }
}
