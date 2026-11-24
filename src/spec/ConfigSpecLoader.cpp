#include "ConfigSpecLoader.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <vector>

#include <nlohmann/json.hpp>

#include "ChoiceOptionsSpec.h"
#include "ConditionSpec.h"
#include "ConfigSpecReading.h"
#include "ContentSpec.h"
#include "FrameworkTypeSpec.h"
#include "RequirementSpec.h"
#include "StorageSpec.h"
#include "common/CommonUtils.h"
#include "model/Categories.h"

using namespace mods_config;

namespace
{
    using namespace spec;

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
    constexpr std::array<std::string_view, 11>
        PAGE_FIELDS{ "pageDisplayName", "help", "id", "content", "contentFromClientCallback", "pages", "defaultFile", "applies", "omitDefault", "visibleWhen", "enabledWhen" };

    /**
     * Whether a page's rows are given by its mod's client (`contentFromClientCallback`). The client is asked by the
     * page's `id`, so a page with none keeps the rows of its own `content`; a page with one has no `content` read.
     */
    bool readContentFromClient(const Json& page, const std::string_view id, ConfigSpecReading& reading)
    {
        const auto flag = page.find("contentFromClientCallback");
        if (flag == page.end()) {
            return false;
        }
        if (!flag->is_boolean()) {
            const Into into(reading, "contentFromClientCallback");
            reading.report(model::Severity::Warning, "Its contentFromClientCallback is not true or false: it is left out.");
            return false;
        }
        if (!flag->get<bool>()) {
            return false;
        }
        if (id.empty()) {
            const Into into(reading, "contentFromClientCallback");
            reading.report(model::Severity::Error, "The page has no id, which its mod's client is asked for its rows by: the client is not asked.");
            return false;
        }
        if (page.contains("content")) {
            const Into into(reading, "content");
            reading.report(model::Severity::Error, "The page's rows are given by its mod's client, as its contentFromClientCallback says: its content is not read.");
        }
        return true;
    }

    /**
     * A page of a config spec, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do. A problem in it is on it: the caller reads it `Into` its place, with its place among the pages,
     * which takes the page back off. A page with an `id` is kept in `contentPages` by it in lower case, with what the
     * content a mod's client gives it inherits and where it is: the first of two with one id.
     */
    model::Page loadPage(const Json& page, const Inherited& above, ConfigSpecReading& reading, std::map<std::string, ContentPage>& contentPages)
    {
        reading.at.pages.push_back(common::trim(getShownText(page, "pageDisplayName", reading.translations)));
        checkFields(page, PAGE_FIELDS, reading);
        model::Page loaded{ .name = readShownText(page, "pageDisplayName", reading),
            .help = readShownText(page, "help", reading),
            .items = {},
            .pages = {},
            .id = getText(page, "id") };
        const auto own = inherit(page, above, reading);
        if (!loaded.id.empty()) {
            contentPages.try_emplace(common::str_tolower(loaded.id), ContentPage{ .inherited = own, .conditions = {}, .names = reading.at.pages, .places = reading.at.pagePlaces });
        }
        loaded.contentFromClient = readContentFromClient(page, loaded.id, reading);
        if (!loaded.contentFromClient) {
            loaded.items = loadItems(page, own, reading);
        }
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loaded.pages.push_back(loadPage((*pages)[place], own, reading, contentPages));
            }
        }
        return loaded;
    }

    /**
     * Gives the sections, the activation spheres and the buttons of a page and of its child pages their `id`.
     */
    void loadPageItemIds(const Json& page, model::Page& loaded, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        if (!loaded.contentFromClient) {
            loadItemIds(page, loaded.items, reading);
        }
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
     * Gives a page, its items and its child pages their conditions, including those of its parent pages. The page
     * kept in `contentPages` gets them too, for the content a mod's client gives it.
     */
    void loadPageConditions(const Json& page, model::Page& loaded, ConfigSpecReading& reading, std::map<std::string, ContentPage>& contentPages, const RowConditions& above = {})
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loaded.visibleWhen = joinConditions(above.visibleWhen, loadRowCondition(page, "visibleWhen", reading));
        loaded.enabledWhen = joinConditions(above.enabledWhen, loadRowCondition(page, "enabledWhen", reading));
        const RowConditions own{ .visibleWhen = loaded.visibleWhen, .enabledWhen = loaded.enabledWhen };
        if (const auto kept = contentPages.find(common::str_tolower(loaded.id)); kept != contentPages.end() && kept->second.places == reading.at.pagePlaces) {
            kept->second.conditions = own;
        }
        if (!loaded.contentFromClient) {
            loadConditions(page, loaded.items, own, reading);
        }
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageConditions((*pages)[place], loaded.pages[place], reading, contentPages, own);
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
     * Hides the rows of the sliders colors are kept in, on pages and the pages under them (`hideColorParts`).
     */
    void hidePagesColorParts(std::vector<model::Page>& pages, const std::set<std::string, std::less<>>& parts)
    {
        for (auto& page : pages) {
            hideColorParts(page.items, parts);
            hidePagesColorParts(page.pages, parts);
        }
    }

    /**
     * Reads what can name a setting of any page, once every setting of the config spec is read: the conditions and the
     * warnings of the rows of the front page and of every page, and then the ids of their sections, spheres and buttons,
     * which no setting's id can be. The rows of the sliders colors are kept in are hidden then.
     */
    void loadConditionsAndIds(const Json& configSpec, model::Mod& mod, ConfigSpecReading& reading, std::map<std::string, ContentPage>& contentPages)
    {
        loadConditions(configSpec, mod.frontPage.items, {}, reading);
        forEachPage(configSpec, reading, [&](const Json& page, const std::size_t place) {
            loadPageConditions(page, mod.pages[place], reading, contentPages);
        });
        loadItemIds(configSpec, mod.frontPage.items, reading);
        forEachPage(configSpec, reading, [&](const Json& page, const std::size_t place) {
            loadPageItemIds(page, mod.pages[place], reading);
        });
        hideColorParts(mod.frontPage.items, reading.colorParts);
        hidePagesColorParts(mod.pages, reading.colorParts);
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
        auto reader = std::make_shared<PageContentReader>();
        mod.frontPage.items = loadItems(configSpec, root, reading);
        forEachPage(configSpec, reading, [&](const Json& page, std::size_t) {
            mod.pages.push_back(loadPage(page, root, reading, reader->pages));
        });
        loadConditionsAndIds(configSpec, mod, reading, reader->pages);
        // what a page's content that a mod's client gives is read against
        reader->modName = mod.name;
        reader->files = reading.files;
        reader->sharedLists = reading.sharedLists ? *reading.sharedLists : Json();
        reader->sharedPresetLists = reading.sharedPresetLists ? *reading.sharedPresetLists : Json();
        reader->settings = std::move(reading.settings);
        reader->itemIds = std::move(reading.itemIds);
        mod.pageContent = std::move(reader);
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
