#include "devbench/MCDevBench.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>

#include <nlohmann/json.hpp>

#include "ModsConfigFO4.h"
#include "api/ModsConfigFO4Api.h"
#include "devbench/DevBench.h"
#include "imgui/ImGuiPointer.h"
#include "model/Conditions.h"
#include "model/TextStates.h"
#include "ui/PageRows.h"

namespace mods_config::devbench
{
    namespace
    {
        using json = nlohmann::json;

        /**
         * One frame of the mod's state. Plain values only: a snapshot outlives its frame.
         */
        struct MCDevState
        {
            bool menuOpen = false;
            bool menuMinimized = false;
            bool frikGestureTaken = false;

            // the pointer, as the frame before saw it
            bool pointerOnPanel = false;
            float pointerX = 0.0f;
            float pointerY = 0.0f;
            bool pointerPrimaryHand = true;
            bool pointerDown = false;
            bool pointerWanted = false;

            // the mods the menu lists
            std::size_t modCount = 0;

            // what the panel shows: the mod as its place in the mods, -1 for none, and the page as its place in
            // the mod's pages and in the pages of each page on the way to it, empty for the mod's front page
            int selectedMod = -1;
            std::vector<std::size_t> selectedPage;
            bool helpShown = false;

            // the dialog asks what to do with the unsaved changes
            bool asking = false;

            // the selected mod's editing session: none on the menu's front page
            bool hasSession = false;
            std::size_t dirtyCount = 0;
            std::size_t invalidCount = 0;
        };

        const char* stateDescription()
        {
            return "menu: open, minimized (the panel is hidden and only the offhand buttons show); "
                   "pointer: onPanel (a wand's ray is on the panel), hand (primary or offhand), "
                   "x and y (where, in the panel's pixels from its top left), down (it presses the panel), wanted (ImGui uses the pointer); "
                   "mods: count (the mods the menu lists; the mods action has each of them); "
                   "selection: mod and page (what the panel shows: the mod as its place from 0 in the mods action's answer, and the page as "
                   "its place in the mod's pages, then in the pages of each page on the way to it; a null mod is the menu's front page, and "
                   "an empty page the mod's front page), helpShown (the Help page is shown over it), asking (the dialog asks to Save, Discard "
                   "or Cancel the unsaved changes before the mod is left or the menu is closed, with Keep and Revert for an MCM mod); "
                   "session: the selected mod's editing session, null on the menu's front page: dirty (the changes that wait for a Save), invalid (the changes that are not valid, "
                   "which block Save); "
                   "frik: gestureTaken (FRIK's open-config binding is turned off)";
        }

        // the state of the frame before, whose changes are published as events: none before the first
        std::optional<MCDevState> lastState;

        /**
         * What a state shows, by names: the mod's name, null for the menu's front page, and the names of the pages on
         * the way to its page, empty for the mod's front page.
         */
        json shownToJson(const MCDevState& state)
        {
            const auto& mods = g_modsConfig.menu().mods();
            if (state.selectedMod < 0 || static_cast<std::size_t>(state.selectedMod) >= mods.size()) {
                return { { "mod", nullptr }, { "page", json::array() } };
            }
            const auto& mod = mods[static_cast<std::size_t>(state.selectedMod)];
            auto names = json::array();
            const auto* pages = &mod.pages;
            for (const auto place : state.selectedPage) {
                if (place >= pages->size()) {
                    break;
                }
                names.push_back((*pages)[place].name);
                pages = &(*pages)[place].pages;
            }
            return { { "mod", mod.name }, { "page", std::move(names) } };
        }

        /**
         * Publishes as devbench events what changed in the menu since the frame before: it opened or closed, was
         * minimized or restored, showed another mod or page, showed or hid the Help page, and its dialog asked or was
         * answered. The panel's own changes are told while the menu stays open.
         */
        void publishChanges(const MCDevState& state)
        {
            if (lastState) {
                const auto& last = *lastState;
                const bool stayedOpen = state.menuOpen && last.menuOpen;
                if (state.menuOpen != last.menuOpen) {
                    f4cf::devbench::emit(state.menuOpen ? "menu.opened" : "menu.closed");
                }
                if (stayedOpen && state.menuMinimized != last.menuMinimized) {
                    f4cf::devbench::emit(state.menuMinimized ? "menu.minimized" : "menu.restored");
                }
                if (state.selectedMod != last.selectedMod || state.selectedPage != last.selectedPage) {
                    f4cf::devbench::emit("selection.changed", [&] {
                        return shownToJson(state);
                    });
                }
                if (stayedOpen && state.helpShown != last.helpShown) {
                    f4cf::devbench::emit(state.helpShown ? "help.shown" : "help.hidden");
                }
                if (stayedOpen && state.asking != last.asking) {
                    f4cf::devbench::emit(state.asking ? "dialog.asked" : "dialog.answered", [&] {
                        return json{ { "mod", shownToJson(state)["mod"] } };
                    });
                }
            }
            lastState = state;
        }

        /**
         * Fills the state of this frame, and publishes what changed in the menu since the frame before.
         */
        void captureState(MCDevState& state)
        {
            const auto& menu = g_modsConfig.menu();
            state.menuOpen = menu.isOpen();
            state.menuMinimized = menu.isMinimized();
            state.frikGestureTaken = g_modsConfig.isFrikGestureTaken();

            const auto& pointer = imgui::pointer().state();
            state.pointerOnPanel = pointer.canvas != nullptr;
            state.pointerX = pointer.x;
            state.pointerY = pointer.y;
            state.pointerPrimaryHand = pointer.primaryHand;
            state.pointerDown = pointer.down;
            state.pointerWanted = pointer.wantsPointer;

            state.modCount = menu.mods().size();
            const auto& selection = menu.selection();
            state.selectedMod = selection.mod ? static_cast<int>(*selection.mod) : -1;
            state.selectedPage = selection.page;
            state.helpShown = menu.isOpen() && g_modsConfig.menu().fullPanel().isHelpShown();
            state.asking = menu.isOpen() && g_modsConfig.menu().fullPanel().isAsking();

            const auto* const store = menu.isOpen() ? g_modsConfig.menu().fullPanel().selectedStore() : nullptr;
            state.hasSession = store != nullptr;
            state.dirtyCount = store ? store->dirtyCount() : 0;
            state.invalidCount = store ? store->invalidCount() : 0;
            publishChanges(state);
        }

        /**
         * A place in a list, which is null for none.
         */
        json placeToJson(const int place)
        {
            return place < 0 ? json(nullptr) : json(place);
        }

        json stateToJson(const MCDevState& state)
        {
            return {
                { "menu", { { "open", state.menuOpen }, { "minimized", state.menuMinimized } } },
                { "pointer",
                    { { "onPanel", state.pointerOnPanel },
                        { "hand", state.pointerPrimaryHand ? "primary" : "offhand" },
                        { "x", state.pointerX },
                        { "y", state.pointerY },
                        { "down", state.pointerDown },
                        { "wanted", state.pointerWanted } } },
                { "mods", { { "count", state.modCount } } },
                { "selection", { { "mod", placeToJson(state.selectedMod) }, { "page", state.selectedPage }, { "helpShown", state.helpShown }, { "asking", state.asking } } },
                { "session", state.hasSession ? json{ { "dirty", state.dirtyCount }, { "invalid", state.invalidCount } } : json(nullptr) },
                { "frik", { { "gestureTaken", state.frikGestureTaken } } },
            };
        }

        json valueToJson(const model::Value& value)
        {
            return std::visit(
                [](const auto& v) {
                    return json(v);
                },
                value);
        }

        const char* settingTypeName(const model::SettingType type)
        {
            switch (type) {
            case model::SettingType::Switcher:
                return "switcher";
            case model::SettingType::Slider:
                return "slider";
            case model::SettingType::Stepper:
                return "stepper";
            case model::SettingType::Dropdown:
                return "dropdown";
            case model::SettingType::TextInput:
                return "textinput";
            case model::SettingType::InputBinding:
                return "cf.inputBinding";
            case model::SettingType::Haptic:
                return "cf.haptic";
            case model::SettingType::Color:
                return "cf.color";
            case model::SettingType::Transform:
                return "cf.transform";
            }
            return "unknown";
        }

        const char* appliesName(const model::Applies applies)
        {
            switch (applies) {
            case model::Applies::Unknown:
                return "unknown";
            case model::Applies::Live:
                return "live";
            case model::Applies::Save:
                return "save";
            case model::Applies::Restart:
                return "restart";
            }
            return "unknown";
        }

        /**
         * The open menu's panel, and the store of the selected mod's session, which a mod has to be selected for.
         */
        model::Store& selectedStore()
        {
            if (!g_modsConfig.menu().isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            auto* const store = g_modsConfig.menu().fullPanel().selectedStore();
            if (!store) {
                throw std::runtime_error("no mod is selected");
            }
            return *store;
        }

        const char* statusLookName(const ui::StatusLook look)
        {
            switch (look) {
            case ui::StatusLook::Plain:
                return "plain";
            case ui::StatusLook::Pending:
                return "pending";
            case ui::StatusLook::Invalid:
                return "invalid";
            case ui::StatusLook::Restart:
                return "restart";
            }
            return "plain";
        }

        /**
         * A text of a row, which is null for none.
         */
        json textOrNull(const std::string_view text)
        {
            return text.empty() ? json(nullptr) : json(std::string(text));
        }

        /**
         * The selected mod's editing session, as the footer counts it, and the status its help line has while the
         * ray is on no row.
         */
        json sessionToJson(const model::Store& store)
        {
            const auto status = g_modsConfig.menu().fullPanel().sessionStatus();
            return { { "dirty", store.dirtyCount() }, { "invalid", store.invalidCount() }, { "status", { { "text", status.text }, { "look", statusLookName(status.look) } } } };
        }

        /**
         * The lines of a text row as it draws them, with the states of its mod's client in them: each line as the
         * text of its runs.
         */
        json linesToJson(const model::Text& text, model::Store& store)
        {
            const auto resolved = model::resolveClientText(text.lines, &store);
            auto lines = json::array();
            for (const auto& line : resolved ? *resolved : text.lines) {
                std::string joined;
                for (const auto& run : line.runs) {
                    joined += run.text;
                }
                lines.push_back(std::move(joined));
            }
            return lines;
        }

        /**
         * Where a setting's value is kept: a file of the selected mod's config spec, with its format, its root, its path
         * and the names that lead to the value in it, a ModSetting by MCM's name, a global or a property of a game plugin,
         * a state of the mod's client, or none.
         */
        json storageToJson(const model::Storage& storage)
        {
            return std::visit(
                []<typename T>(const T& kept) -> json {
                    if constexpr (std::is_same_v<T, model::FileStorage>) {
                        json file = { { "kind", "file" }, { "file", kept.file }, { "path", kept.path } };
                        const auto& panel = g_modsConfig.menu().fullPanel();
                        if (const auto& selection = panel.selection(); selection.mod) {
                            const auto& files = panel.mods()[*selection.mod].files;
                            if (const auto found = files.find(kept.file); found != files.end()) {
                                file["format"] = found->second.format == model::FileFormat::Ini ? "ini" : "toml";
                                file["root"] = found->second.root == model::FileRoot::Data ? "data" : "mygames";
                                file["filePath"] = found->second.path;
                            }
                        }
                        return file;
                    } else if constexpr (std::is_same_v<T, model::ModSettingStorage>) {
                        return { { "kind", "modSetting" }, { "name", kept.name } };
                    } else if constexpr (std::is_same_v<T, model::GlobalStorage>) {
                        return { { "kind", "global" }, { "plugin", kept.plugin }, { "formId", std::format("{:X}", kept.formId) } };
                    } else if constexpr (std::is_same_v<T, model::PropertyStorage>) {
                        return { { "kind", "property" },
                            { "plugin", kept.plugin },
                            { "formId", std::format("{:X}", kept.formId) },
                            { "script", kept.script },
                            { "name", kept.name } };
                    } else if constexpr (std::is_same_v<T, model::ClientStateStorage>) {
                        return { { "kind", "clientState" }, { "name", kept.name } };
                    } else if constexpr (std::is_same_v<T, model::UnknownStorage>) {
                        return { { "kind", "unknown" }, { "type", kept.type } };
                    } else {
                        return { { "kind", "none" } };
                    }
                },
                storage);
        }

        /**
         * A setting as its row shows it: its text and help with the states of its mod's client in them, its value
         * in the store, whether that is available, invalid or dirty and why, the warning it tells, the tag of when
         * its pending change takes effect, whether its row is enabled or read-only, and what the player can set it to.
         */
        json settingToJson(const model::Setting& setting, model::Store& store)
        {
            const auto stored = store.value(setting);
            const bool dirty = store.isDirty(setting);
            // an invalid value has its mark in the place of a warning's, and the tag is told while the change is pending
            const auto warning = stored.invalid ? std::string_view() : ui::warningOf(setting.warnings, &store);
            const auto applies = dirty ? store.applies(setting) : std::nullopt;
            json row = { { "kind", "setting" },
                { "id", setting.id },
                { "type", settingTypeName(setting.type) },
                { "text", model::resolveClientText(setting.text, &store) },
                { "help", model::resolveClientText(setting.help, &store) },
                { "warning", textOrNull(warning) },
                { "tag", textOrNull(applies ? ui::appliesTag(*applies) : std::string_view()) },
                { "value", valueToJson(stored.value) },
                { "available", stored.available },
                { "invalid", stored.invalid },
                { "reason", stored.reason.empty() ? json(nullptr) : json(stored.reason) },
                { "dirty", dirty },
                { "readOnly", setting.readOnly },
                { "enabled", !setting.enabledWhen || model::holds(*setting.enabledWhen, &store) },
                { "storage", storageToJson(setting.storage) } };
            const auto defaultValue = store.defaultValue(setting);
            row["default"] = defaultValue ? valueToJson(*defaultValue) : json(nullptr);
            const auto saved = store.savedValue(setting);
            row["saved"] = saved ? valueToJson(*saved) : json(nullptr);
            row["applies"] = appliesName(setting.applies);
            if (setting.type == model::SettingType::Slider) {
                row["range"] = { { "min", setting.valueOptions.min }, { "max", setting.valueOptions.max }, { "step", setting.valueOptions.step } };
            } else if (const auto& choiceOptions = store.options(setting); !choiceOptions.empty()) {
                auto options = json::array();
                for (const auto& option : choiceOptions) {
                    options.push_back({ { "value", valueToJson(option.value) }, { "text", option.text } });
                }
                row["options"] = std::move(options);
            }
            return row;
        }

        /**
         * The rows of items, in the order they are drawn, a section's after its heading with a depth one more.
         * A row whose condition does not hold is listed with `shown` false, where the panel gives it no room.
         */
        void addRows(const std::vector<model::Item>& items, const int depth, model::Store& store, json& rows)
        {
            const auto shown = [&store](const std::optional<model::Condition>& condition) {
                return !condition || model::holds(*condition, &store);
            };
            for (const auto& item : items) {
                json row = std::visit(
                    [&]<typename T>(const T& it) -> json {
                        if constexpr (std::is_same_v<T, model::Setting>) {
                            return settingToJson(it, store);
                        } else if constexpr (std::is_same_v<T, model::Group>) {
                            return { { "kind", "section" },
                                { "text", model::resolveClientText(it.text, &store) },
                                { "help", model::resolveClientText(it.help, &store) },
                                { "role", it.role == model::GroupRole::ActivationSphere ? json("activationSphere") : json(nullptr) },
                                { "collapsible", it.collapsible },
                                { "collapsed", it.collapsed },
                                { "folded", it.collapsible && g_modsConfig.menu().fullPanel().isFolded(it) } };
                        } else if constexpr (std::is_same_v<T, model::Button>) {
                            return { { "kind", "button" },
                                { "text", model::resolveClientText(it.text, &store) },
                                { "help", model::resolveClientText(it.help, &store) },
                                { "warning", textOrNull(ui::warningOf(it.warnings, &store)) },
                                { "buttonText", it.buttonText },
                                { "runnable",
                                    !std::holds_alternative<model::NoAction>(it.action) && !std::holds_alternative<model::UnknownAction>(it.action) &&
                                        store.notRunnable(it.action).empty() },
                                { "notRunnable", store.notRunnable(it.action) },
                                { "confirmation", it.confirmation.has_value() },
                                { "enabled", shown(it.enabledWhen) } };
                        } else if constexpr (std::is_same_v<T, model::Hotkey>) {
                            const auto key = store.hotkey(it);
                            return { { "kind", "hotkey" },
                                { "id", it.id },
                                { "text", it.text },
                                { "help", it.help },
                                { "value", valueToJson(key.value) },
                                { "available", key.available } };
                        } else if constexpr (std::is_same_v<T, model::Text>) {
                            return { { "kind", "text" }, { "lines", linesToJson(it, store) } };
                        } else if constexpr (std::is_same_v<T, model::Image>) {
                            return { { "kind", "image" }, { "read", it.pixels != nullptr }, { "texture", it.texture }, { "width", it.width }, { "height", it.height } };
                        } else if constexpr (std::is_same_v<T, model::Spacer>) {
                            return { { "kind", "spacer" }, { "lines", it.lines } };
                        } else {
                            return { { "kind", "placeholder" }, { "text", it.text }, { "type", it.type } };
                        }
                    },
                    item);
                row["depth"] = depth;
                row["shown"] = std::visit(
                    [&](const auto& it) {
                        return shown(it.visibleWhen);
                    },
                    item);
                rows.push_back(std::move(row));
                if (const auto* group = std::get_if<model::Group>(&item)) {
                    addRows(group->items, depth + 1, store, rows);
                }
            }
        }

        const model::Setting* findSetting(const std::vector<model::Item>& items, const std::string_view id)
        {
            for (const auto& item : items) {
                if (const auto* setting = std::get_if<model::Setting>(&item); setting && setting->id == id) {
                    return setting;
                }
                if (const auto* group = std::get_if<model::Group>(&item)) {
                    if (const auto* found = findSetting(group->items, id)) {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        const model::Setting* findSetting(const model::Page& page, const std::string_view id)
        {
            if (const auto* found = findSetting(page.items, id)) {
                return found;
            }
            for (const auto& sub : page.pages) {
                if (const auto* found = findSetting(sub, id)) {
                    return found;
                }
            }
            return nullptr;
        }

        /**
         * The setting of an id in the selected mod, on any of its pages, and none while the menu is closed, no mod is
         * selected or the mod has no such setting.
         */
        const model::Setting* selectedSettingOf(const std::string_view id)
        {
            auto& menu = g_modsConfig.menu();
            const auto& panel = menu.fullPanel();
            if (!menu.isOpen() || !panel.selection().mod) {
                return nullptr;
            }
            const auto& mod = panel.mods()[*panel.selection().mod];
            const auto* setting = findSetting(mod.frontPage, id);
            for (std::size_t i = 0; !setting && i < mod.pages.size(); ++i) {
                setting = findSetting(mod.pages[i], id);
            }
            return setting;
        }

        /**
         * The setting of an id in the selected mod, on any of its pages.
         */
        const model::Setting& selectedSetting(const json& args)
        {
            const auto id = args.value("id", std::string());
            if (id.empty()) {
                throw std::runtime_error("no id is given");
            }
            const auto* setting = selectedSettingOf(id);
            if (!setting) {
                throw std::runtime_error("the mod has no setting with the id '" + id + "'");
            }
            return *setting;
        }

        /**
         * A value of the request in the type of a setting: a bool for a switcher, a number for a slider, a text
         * for a text input, the TOML text of a binding's, a color's and a transform's table or its INI line, the name of a haptic's pattern, and for
         * a choice the value of one of the options its store has, or its text.
         */
        model::Value valueFromJson(const model::Setting& setting, const model::Store& store, const json& given)
        {
            // the tool's schema has `value` as a text, which the generic set action declares it as: a text that is a
            // JSON value is that value, as `true` or `2.5`
            json value = given;
            if (given.is_string() && setting.type != model::SettingType::TextInput) {
                value = json::parse(given.get<std::string>(), nullptr, false);
                if (value.is_discarded()) {
                    value = given;
                }
            }
            switch (setting.type) {
            case model::SettingType::Switcher:
                if (!value.is_boolean()) {
                    throw std::runtime_error("a switcher's value is true or false");
                }
                return value.get<bool>();
            case model::SettingType::Slider:
                if (!value.is_number()) {
                    throw std::runtime_error("a slider's value is a number");
                }
                return value.get<float>();
            case model::SettingType::TextInput:
                return value.is_string() ? value.get<std::string>() : value.dump();
            case model::SettingType::InputBinding:
            case model::SettingType::Color:
            case model::SettingType::Transform:
                // the TOML text of the table, or its line in an INI file, which is no JSON value
                return given.is_string() ? given.get<std::string>() : given.dump();
            case model::SettingType::Haptic:
                // the name of a pattern, which the store reads as the framework does, and one it does not have is invalid
                if (!given.is_string()) {
                    throw std::runtime_error("a haptic's value is the name of a pattern, or none");
                }
                return given.get<std::string>();
            case model::SettingType::Stepper:
            case model::SettingType::Dropdown:
                for (const auto& option : store.options(setting)) {
                    if (valueToJson(option.value) == value || valueToJson(option.value) == given || (given.is_string() && option.text == given.get<std::string>())) {
                        return option.value;
                    }
                }
                throw std::runtime_error("the choice has no option of the value or the text " + value.dump());
            }
            throw std::runtime_error("the setting's type is not known");
        }

        /**
         * The rows of the page that is shown, with the values the store has for them, and the session.
         */
        json runPage(const json&)
        {
            auto& store = selectedStore();
            const auto& panel = g_modsConfig.menu().fullPanel();
            const auto& mod = panel.mods()[*panel.selection().mod];
            const auto& page = *panel.selectedPage();
            auto rows = json::array();
            addRows(page.items, 0, store, rows);
            return { { "mod", mod.name }, { "page", page.name }, { "helpShown", panel.isHelpShown() }, { "session", sessionToJson(store) }, { "rows", std::move(rows) } };
        }

        /**
         * Sets a setting's value in the store, as its control does: the answer has the setting as its row shows it then.
         */
        json runSetValue(const json& args)
        {
            auto& store = selectedStore();
            const auto& setting = selectedSetting(args);
            if (!args.contains("value")) {
                throw std::runtime_error("no value is given");
            }
            const bool set = store.set(setting, valueFromJson(setting, store, args["value"]));
            return { { "set", set }, { "setting", settingToJson(setting, store) }, { "session", sessionToJson(store) } };
        }

        /**
         * A value of the request as a client gives it: a number in the valueType of its setting, and a text as it is for
         * a setting of a text. With no such setting in the selected mod, as for a value the menu is to refuse, a number
         * has its own JSON type. A text that is a JSON value is that value, as setValue has it.
         */
        api::Value clientValueFromJson(const model::Setting* setting, const json& given)
        {
            json value = given;
            if (given.is_string() && !(setting && setting->valueType == model::StoredType::String)) {
                value = json::parse(given.get<std::string>(), nullptr, false);
                if (value.is_discarded()) {
                    value = given;
                }
            }
            if (value.is_boolean()) {
                return value.get<bool>();
            }
            if (value.is_number()) {
                const auto type = setting ? setting->valueType : value.is_number_integer() ? model::StoredType::Int : model::StoredType::Float;
                if (type == model::StoredType::Int) {
                    return static_cast<int>(std::lround(value.get<double>()));
                }
                if (type == model::StoredType::Float) {
                    return value.get<float>();
                }
                // a number of a setting of a bool or a text, which the menu refuses
                return value.is_number_integer() ? api::Value(value.get<int>()) : api::Value(value.get<float>());
            }
            return value.is_string() ? value.get<std::string>() : value.dump();
        }

        /**
         * Sets a pending value as this mod's own client does through the native API, as a mod's own UI hands its edits
         * back: the menu takes it only while it shows Mods Config FO4 VR, and its log says why it refused one. The answer
         * has the setting as its row shows it then, while the selected mod has it.
         */
        json runClientSetValue(const json& args)
        {
            const auto id = args.value("id", std::string());
            if (id.empty()) {
                throw std::runtime_error("no id is given");
            }
            if (!args.contains("value")) {
                throw std::runtime_error("no value is given");
            }
            auto& client = api::ModsConfigFO4Client::get();
            if (!client.isConnected()) {
                throw std::runtime_error("the mod's client is not connected to the menu");
            }
            const auto* setting = selectedSettingOf(id);
            json answer{ { "set", client.setPendingValue(id, clientValueFromJson(setting, args["value"])) } };
            if (setting) {
                auto& store = selectedStore();
                answer["setting"] = settingToJson(*setting, store);
                answer["session"] = sessionToJson(store);
            }
            return answer;
        }

        /**
         * Sets a setting's default, as its Reset does.
         */
        json runReset(const json& args)
        {
            auto& store = selectedStore();
            const auto& setting = selectedSetting(args);
            const auto defaultValue = store.defaultValue(setting);
            if (!defaultValue) {
                throw std::runtime_error("the setting has no default");
            }
            const bool set = store.set(setting, *defaultValue);
            return { { "set", set }, { "setting", settingToJson(setting, store) }, { "session", sessionToJson(store) } };
        }

        json runSave(const json&)
        {
            auto& store = selectedStore();
            const auto saved = store.save();
            return { { "saved", saved.has_value() }, { "reason", saved ? std::string() : saved.error() }, { "session", sessionToJson(store) } };
        }

        /**
         * Takes back the selected mod's changes, as the footer's Discard does, or the change of the setting of an id
         * alone, as its row's Discard does.
         */
        json runDiscard(const json& args)
        {
            auto& store = selectedStore();
            if (!args.contains("id")) {
                store.discard();
                return { { "session", sessionToJson(store) } };
            }
            const auto& setting = selectedSetting(args);
            store.discard(setting);
            return { { "setting", settingToJson(setting, store) }, { "session", sessionToJson(store) } };
        }

        /**
         * Whether a row's text is a text of the request: its own, or as it is drawn, with the states of its mod's
         * client in it.
         */
        bool isTextOf(const std::string& own, const std::string_view text, model::Store& store)
        {
            return own == text || model::resolveClientText(own, &store) == text;
        }

        /**
         * The button of a text among items and the sections in them, that is shown: a row the panel gives no room is
         * not pressed. None where there is no such button.
         */
        const model::Button* findButton(const std::vector<model::Item>& items, const std::string_view text, model::Store& store)
        {
            for (const auto& item : items) {
                const bool shown = std::visit(
                    [&store](const auto& it) {
                        return !it.visibleWhen || model::holds(*it.visibleWhen, &store);
                    },
                    item);
                if (!shown) {
                    continue;
                }
                if (const auto* button = std::get_if<model::Button>(&item); button && isTextOf(button->text, text, store)) {
                    return button;
                }
                if (const auto* group = std::get_if<model::Group>(&item)) {
                    if (const auto* found = findButton(group->items, text, store)) {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        /**
         * Runs the action of a button of the page that is shown, by its text, as a press on its row and then on the
         * confirming button of its confirmation does: a script's function saves the unsaved changes first, as Save and
         * run does, and an invalid one or a Save that fails runs nothing, unless it is run without saving. A button
         * whose action the menu or its store does not run, or whose enabledWhen does not hold, is disabled, and is
         * not run. The answer has what the run did.
         */
        json runButton(const json& args)
        {
            auto& store = selectedStore();
            const auto text = args.value("text", std::string());
            if (text.empty()) {
                throw std::runtime_error("no text is given");
            }
            const auto* button = findButton(g_modsConfig.menu().fullPanel().selectedPage()->items, text, store);
            if (!button) {
                throw std::runtime_error("the page that is shown has no button '" + text + "'");
            }
            if (const auto* unknown = std::get_if<model::UnknownAction>(&button->action)) {
                throw std::runtime_error("the menu does not run its action: " + unknown->reason);
            }
            if (std::holds_alternative<model::NoAction>(button->action)) {
                throw std::runtime_error("the button has no action");
            }
            if (button->enabledWhen && !model::holds(*button->enabledWhen, &store)) {
                throw std::runtime_error("the button is disabled: its enabledWhen does not hold");
            }
            if (const auto reason = store.notRunnable(button->action); !reason.empty()) {
                throw std::runtime_error("the button is disabled: " + reason);
            }
            const bool saveFirst = std::holds_alternative<model::CallFunction>(button->action) && store.dirtyCount() > 0 && !args.value("withoutSaving", false);
            if (saveFirst && store.invalidCount() > 0) {
                throw std::runtime_error("the unsaved changes are saved first, and one of them is not valid");
            }
            if (saveFirst) {
                if (const auto saved = store.save(); !saved) {
                    throw std::runtime_error("the unsaved changes are saved first, and are not saved: " + saved.error());
                }
            }
            const auto result = store.run(button->action);
            return { { "ran", result.ran }, { "failed", result.failed }, { "message", result.message }, { "session", sessionToJson(store) } };
        }

        /**
         * The section of a text among items and the sections in them, and none where there is no such section.
         */
        const model::Group* findSection(const std::vector<model::Item>& items, const std::string_view text, model::Store& store)
        {
            for (const auto& item : items) {
                if (const auto* group = std::get_if<model::Group>(&item)) {
                    if (isTextOf(group->text, text, store)) {
                        return group;
                    }
                    if (const auto* found = findSection(group->items, text, store)) {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        /**
         * Folds a section of the page that is shown, by its text, or opens it, as a press on its heading does.
         */
        json runFold(const json& args)
        {
            auto& store = selectedStore();
            const auto text = args.value("text", std::string());
            if (text.empty()) {
                throw std::runtime_error("no text is given");
            }
            auto& panel = g_modsConfig.menu().fullPanel();
            const auto* section = findSection(panel.selectedPage()->items, text, store);
            if (!section) {
                throw std::runtime_error("the page that is shown has no section '" + text + "'");
            }
            if (!section->collapsible) {
                throw std::runtime_error("the section does not fold");
            }
            panel.setFolded(*section, args.value("folded", true));
            return { { "folded", panel.isFolded(*section) } };
        }

        /**
         * Presses a button of the dialog that asks what to do with the selected mod's unsaved changes: Save,
         * Discard or Cancel. The answer has whether the dialog still asks, and what the panel shows then.
         */
        json runAnswer(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            auto& panel = menu.fullPanel();
            if (!panel.isAsking()) {
                throw std::runtime_error("the dialog does not ask");
            }
            const auto name = args.value("button", std::string());
            const auto answer = name == "save"      ? ui::DialogAnswer::Save
                                : name == "discard" ? ui::DialogAnswer::Discard
                                : name == "cancel"  ? ui::DialogAnswer::Cancel
                                                    : ui::DialogAnswer::None;
            if (answer == ui::DialogAnswer::None) {
                throw std::runtime_error("the button is save, discard or cancel");
            }
            const bool pressed = panel.answerDialog(answer);
            const auto& selection = panel.selection();
            auto* store = panel.selectedStore();
            return { { "pressed", pressed },
                { "asking", panel.isAsking() },
                { "mod", selection.mod ? json(*selection.mod) : json(nullptr) },
                { "page", selection.page },
                { "session", store ? sessionToJson(*store) : json(nullptr) },
                { "saveFailure", store ? json(std::string(store->saveFailure())) : json(nullptr) } };
        }

        /**
         * Shows the Help page over what is selected, as the title bar's Help does, or the selection's page again.
         */
        json runHelp(const json& args)
        {
            if (!g_modsConfig.menu().isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            auto& panel = g_modsConfig.menu().fullPanel();
            panel.showHelp(args.value("shown", true));
            return { { "helpShown", panel.isHelpShown() } };
        }

        /**
         * Hides the panel, as Minimize does, or shows it again in front of the player, as Restore does.
         */
        json runMinimize(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            menu.setMinimized(args.value("minimized", true));
            return { { "minimized", menu.isMinimized() } };
        }

        json runOpen(const json&)
        {
            if (!ModsConfigFO4::isPlayerLoaded()) {
                throw std::runtime_error("no save is loaded");
            }
            auto& menu = g_modsConfig.menu();
            menu.open();
            return { { "open", menu.isOpen() } };
        }

        /**
         * Closes the menu at once, its unsaved changes dropped, or with ask as the player closes it, with the gesture or
         * Exit on the wrist: the dialog asks first while the mod that is shown has unsaved changes, and with none the
         * menu closes on the next frame.
         */
        json runClose(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (args.value("ask", false)) {
                menu.askToClose();
            } else {
                menu.close();
            }
            return { { "open", menu.isOpen() }, { "asking", menu.isOpen() && menu.fullPanel().isAsking() } };
        }

        /**
         * One step back in the open menu, as the Back button does: the answer has what the panel shows then,
         * and whether the menu is still open.
         */
        json runBack(const json&)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            menu.back();
            const auto& selection = menu.selection();
            return { { "open", menu.isOpen() }, { "mod", selection.mod ? json(*selection.mod) : json(nullptr) }, { "page", selection.page } };
        }

        /**
         * Presses a breadcrumb of the title bar, as the pointer does, by its place as the title bar draws it: 0 is the
         * menu's, 1 the mod's, and 2 on the pages' on the way to the page that is shown. A crumb before the last is a
         * link and the last is where the panel is; over the Help page and a report the menu's crumb is the only link
         * here. The answer has what the panel shows then, and whether the dialog asks first.
         */
        json runBreadcrumb(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }
            auto& panel = menu.fullPanel();
            const auto& selection = panel.selection();
            const bool overPage = panel.isHelpShown() || panel.report().has_value();
            const auto level = args.value("level", -1);
            const std::size_t crumbs = overPage || !selection.mod ? 1 : 2 + selection.page.size();
            if (level < 0 || static_cast<std::size_t>(level) >= crumbs) {
                throw std::runtime_error(std::format("the title bar has no crumb {} that is pressed here", level));
            }
            const bool link = level == 0 ? selection.mod.has_value() || overPage : static_cast<std::size_t>(level) + 1 < crumbs;
            if (!link) {
                throw std::runtime_error("the crumb is where the panel is, which is no link");
            }
            ui::Selection pressed;
            if (level > 0) {
                pressed.mod = selection.mod;
                pressed.page.assign(selection.page.begin(), selection.page.begin() + (level - 1));
            }
            menu.select(pressed);
            const auto& shown = menu.selection();
            return { { "mod", shown.mod ? json(*shown.mod) : json(nullptr) }, { "page", shown.page }, { "helpShown", panel.isHelpShown() }, { "asking", panel.isAsking() } };
        }

        /**
         * Pages by their names, in order: a page with pages under it is its name and its pages.
         */
        json pagesToJson(const std::vector<model::Page>& pages)
        {
            auto names = json::array();
            for (const auto& page : pages) {
                names.push_back(page.pages.empty() ? json(page.name) : json{ { "name", page.name }, { "pages", pagesToJson(page.pages) } });
            }
            return names;
        }

        /**
         * The mods the menu lists, in its order: each with its names, its pages, why it is not available, which is
         * null for one that is, and how many of its saved changes take effect after a restart, which the menu's
         * front page tells while there is one.
         */
        json runMods(const json&)
        {
            const auto& panel = g_modsConfig.menu().fullPanel();
            const auto& listed = panel.mods();
            const auto noticed = panel.frontPageMods();
            auto mods = json::array();
            for (std::size_t i = 0; i < listed.size(); ++i) {
                const auto& mod = listed[i];
                mods.push_back({ { "name", mod.name },
                    { "displayName", mod.displayName },
                    { "pages", pagesToJson(mod.pages) },
                    { "notAvailable", mod.notAvailable.empty() ? json(nullptr) : json(mod.notAvailable) },
                    { "restarts", noticed[i].savedRestartCount } });
            }
            return { { "mods", std::move(mods) } };
        }

        /**
         * A problem of a config spec, as its report lists it: the row it is on, with where that is among the pages
         * and in spec.json, and the place of the row in its mod, which is null for a problem of a page or the file.
         */
        json problemToJson(const model::Problem& problem)
        {
            return { { "severity", problem.severity == model::Severity::Error ? "error" : "warning" },
                { "row", problem.row },
                { "message", problem.message },
                { "where", problem.where },
                { "path", problem.path },
                { "place", problem.place ? json{ { "page", problem.place->page }, { "item", problem.place->item } } : json(nullptr) } };
        }

        /**
         * The errors and the warnings of each config spec the menu read, in the order of the mods, as the Developer
         * tools list them, whether they are on or not: with a mod's name, the one config spec of that name. Each has
         * whether it is loaded, why it is not available, its live copy and the last read of it, and its problems.
         */
        json runReport(const json& args)
        {
            const auto& panel = g_modsConfig.menu().fullPanel();
            const auto modName = args.value("mod", std::string());
            auto configSpecs = json::array();
            for (const auto& mod : panel.mods()) {
                if (mod.source != model::Source::ConfigSpec || (!modName.empty() && mod.name != modName)) {
                    continue;
                }
                std::size_t errors = 0;
                auto problems = json::array();
                for (const auto& problem : mod.problems) {
                    errors += problem.severity == model::Severity::Error ? 1 : 0;
                    problems.push_back(problemToJson(problem));
                }
                const auto* read = mod.folder.empty() ? nullptr : panel.liveCopyRead(mod.folder);
                configSpecs.push_back({ { "name", mod.name },
                    { "displayName", mod.displayName },
                    { "folder", mod.folder },
                    // one that cannot be read is not available, with its error
                    { "loaded", mod.notAvailable.empty() || errors == 0 },
                    { "notAvailable", textOrNull(mod.notAvailable) },
                    { "liveCopy", textOrNull(mod.liveCopy) },
                    { "liveCopyRead", read ? json{ { "result", read->result }, { "failed", read->failed }, { "stale", read->stale } } : json(nullptr) },
                    { "errors", errors },
                    { "warnings", mod.problems.size() - errors },
                    { "problems", std::move(problems) } });
            }
            if (!modName.empty() && configSpecs.empty()) {
                throw std::runtime_error("no config spec is named '" + modName + "'");
            }
            return { { "configSpecs", std::move(configSpecs) } };
        }

        /**
         * Shows a mod's front page or one of its pages, as a press on its row does: the mod by its name, and
         * the page by its name, or a page under a page by the names of the pages on the way to it. With no mod
         * it is the menu's front page. A mod that is not available is not shown, as its row is not pressed.
         * The answer has what the panel shows then, and whether the dialog asks about unsaved changes first.
         */
        json runSelect(const json& args)
        {
            auto& menu = g_modsConfig.menu();
            if (!menu.isOpen()) {
                throw std::runtime_error("the menu is not open");
            }

            ui::Selection selection;
            if (const auto modName = args.value("mod", std::string()); !modName.empty()) {
                const auto& mods = menu.mods();
                const auto mod = std::ranges::find(mods, modName, &model::Mod::name);
                if (mod == mods.end()) {
                    throw std::runtime_error("no mod is named '" + modName + "'");
                }
                if (!mod->notAvailable.empty()) {
                    throw std::runtime_error("the mod is not available: " + mod->notAvailable);
                }
                selection.mod = static_cast<std::size_t>(mod - mods.begin());

                auto pageNames = args.value("page", json());
                if (pageNames.is_string()) {
                    pageNames = json::array({ pageNames });
                }
                const auto* pages = &mod->pages;
                for (const auto& pageName : pageNames.is_array() ? pageNames : json::array()) {
                    const auto name = pageName.is_string() ? pageName.get<std::string>() : std::string();
                    const auto page = std::ranges::find(*pages, name, &model::Page::name);
                    if (page == pages->end()) {
                        throw std::runtime_error("no page named '" + name + "' is there");
                    }
                    selection.page.push_back(static_cast<std::size_t>(page - pages->begin()));
                    pages = &page->pages;
                }
            }
            menu.select(selection);
            // what the panel shows: the dialog can ask first, and keep the mod that has unsaved changes
            const auto& shown = menu.selection();
            return { { "mod", shown.mod ? json(*shown.mod) : json(nullptr) }, { "page", shown.page }, { "asking", menu.fullPanel().isAsking() } };
        }
    }

    void setupDevBenchTool()
    {
        f4cf::devbench::setToolDescription(
            "Mods Config FO4 VR: the in-game configuration menu for mods - open, close and minimize the menu, read its state, the mods it lists and the errors and warnings of "
            "each config spec, show a mod, one of "
            "its pages or the Help page, go back, press a breadcrumb, read the rows of the page as they are drawn with their values, fold a section, set a value or its default, "
            "set a value as the "
            "mod's own "
            "client does through the API, run a button, Save or Discard the "
            "changes, and answer the dialog that asks about unsaved changes. Its events: menu.opened, menu.closed, menu.minimized, menu.restored, "
            "selection.changed (mod and page names), help.shown, help.hidden, dialog.asked, dialog.answered, session.saved (mod, saved, reason), "
            "session.discarded (mod, and setting for a row's Discard), client.event, client.change and client.saved (what a mod's native client was told, "
            "and whether it was), mcm.event (an MCM event sent to the scripts, with its arguments) and mcm.pauseMenu (the pause menu's events sent after "
            "an MCM Save)");

        f4cf::devbench::setStateProvider<MCDevState>(stateDescription(), &captureState, &stateToJson);

        f4cf::devbench::addAction({
            .name = "open",
            .description = "open the config menu",
            .handler = &runOpen,
        });
        f4cf::devbench::addAction({
            .name = "close",
            .description = "close the config menu at once, its unsaved changes dropped, or with ask as the player closes it: the dialog asks first "
                           "while the mod that is shown has unsaved changes, and with none it closes on the next frame",
            .arguments = { { "ask", { { "type", "boolean" }, { "description", "close: close as the player does, with the gesture or Exit on the wrist" } } } },
            .handler = &runClose,
        });
        f4cf::devbench::addAction({
            .name = "back",
            .description = "one step back in the open menu, as the Back button: a page to the page it is under or to its mod's front page, that to the menu's "
                           "front page, and from there the menu closes",
            .handler = &runBack,
        });
        f4cf::devbench::addAction({
            .name = "breadcrumb",
            .description = "press a breadcrumb of the title bar, as the pointer does: one before the last is a link, and the dialog asks first when it leaves a mod "
                           "with unsaved changes; over the Help page and a report only the menu's crumb is pressed",
            .arguments = { { "level",
                { { "type", "integer" },
                    { "description", "breadcrumb: the crumb's place, 0 the menu's, 1 the mod's, 2 and on the pages' on the way to the page that is shown" } } } },
            .handler = &runBreadcrumb,
        });
        f4cf::devbench::addAction({
            .name = "mods",
            .description = "the mods the menu lists, in its order: each with its name, its display name, its pages (a page with pages under it is an object "
                           "with its name and its pages), why it is not available (null for one that is), and restarts (its saved changes that take effect "
                           "after a restart, which the menu's front page tells while there is one)",
            .handler = &runMods,
        });
        f4cf::devbench::addAction({
            .name = "report",
            .description = "the errors and the warnings of each config spec the menu read, in its order, as the Developer tools list them, on or not: each with its "
                           "name, folder, loaded (false for one that cannot be read), notAvailable, liveCopy (the copy it is read from, null for none), liveCopyRead "
                           "(the last read of it: result, failed and stale, null for none), errors, warnings, and its problems in the order of its spec.json: "
                           "severity, row, message, where (its pages and row), path (in spec.json) and place (page and item, null for a problem of a page or the file)",
            .arguments = { { "mod", { { "type", "string" }, { "description", "report: one config spec by its name; none for every one" } } } },
            .handler = &runReport,
        });
        f4cf::devbench::addAction({
            .name = "select",
            .description = "show a mod's front page or one of its pages in the open menu, or the menu's front page with no mod",
            .arguments = { { "mod", { { "type", "string" }, { "description", "select: the mod's name, as the mods action has it; none for the menu's front page" } } },
                { "page",
                    { { "type", { "string", "array" } },
                        { "items", { { "type", "string" } } },
                        { "description",
                            "select: the name of a page of the mod, as the mods action has it, or the names of the pages on the way to a page under a page; "
                            "none for the mod's front page" } } } },
            .handler = &runSelect,
        });
        f4cf::devbench::addAction({
            .name = "page",
            .description = "the rows of the page that is shown, in their order, as they are drawn, with the session (dirty: the changes that wait for a Save, "
                           "status: the help line's text and look while the ray is on no row): each with its kind and depth (a section's rows follow it, one "
                           "deeper), shown (its condition holds), and its text and help with the states of its mod's client in them; for a section its role "
                           "(activationSphere for an activation sphere's group, else null), collapsible (it folds), collapsed (it starts folded) and folded (it is "
                           "folded now), for a button its warning (null for none), buttonText, runnable and enabled, for a text its lines, and for a setting its id, "
                           "type, value, available, invalid, reason, warning (null for none), tag (the help line's tag of when its pending change takes effect, "
                           "null while it is not dirty or that is not known), dirty, saved (the value its row's Discard gives a dirty one back), enabled, default, "
                           "applies, storage (where its value is kept: kind file with its file, format, root, filePath and the path to the value in it, "
                           "modSetting with MCM's name, global or property with its plugin and formId, clientState, unknown or none), and its range or options",
            .handler = &runPage,
        });
        f4cf::devbench::addAction({
            .name = "setValue",
            .description = "set a setting of the selected mod in its store, as its control does: the change waits for a Save",
            .arguments = { { "id", { { "type", "string" }, { "description", "setValue, clientSetValue, reset, discard: the setting's id, as the page action has it" } } },
                { "value",
                    { { "description",
                        "setValue: true or false for a switcher, a number for a slider, a text for a text input, an option's value or text for a choice; "
                        "clientSetValue: true or false, a number, which is sent in the setting's valueType, or a text, which is a color's, a transform's "
                        "and a binding's TOML table" } } } },
            .handler = &runSetValue,
        });
        f4cf::devbench::addAction({
            .name = "clientSetValue",
            .description = "set a setting's pending value as this mod's own native client does through the API, as a mod's own UI hands its edits back: "
                           "the menu takes it only while it shows Mods Config FO4 VR, open or minimized, and not while its dialog asks. set is false for one "
                           "it refused, and the log says why; a live one is told back to the client, which previews it",
            .handler = &runClientSetValue,
        });
        f4cf::devbench::addAction({
            .name = "reset",
            .description = "set a setting of the selected mod to its default, as its Reset does",
            .handler = &runReset,
        });
        f4cf::devbench::addAction({
            .name = "save",
            .description = "write the selected mod's changes into its files, as the footer's Save does",
            .handler = &runSave,
        });
        f4cf::devbench::addAction({
            .name = "discard",
            .description = "take back the selected mod's changes, as the footer's Discard does, or with an id that setting's change alone, as its row's Discard does",
            .handler = &runDiscard,
        });
        f4cf::devbench::addAction({
            .name = "run",
            .description = "run the action of a button of the page that is shown, by its text, as a press on its row does: a disabled button is not run, "
                           "and a script's function saves the unsaved changes first, as Save and run does",
            .arguments = { { "text", { { "type", "string" }, { "description", "run, fold: the button's or the section's text, as the page action has it" } } },
                { "withoutSaving", { { "type", "boolean" }, { "description", "run: run a script's function with the unsaved changes pending, as Run without saving does" } } } },
            .handler = &runButton,
        });
        f4cf::devbench::addAction({
            .name = "fold",
            .description = "fold a section of the page that is shown, or open it, as a press on its heading does",
            .arguments = { { "folded", { { "type", "boolean" }, { "description", "fold: false opens the section; true by default" } } } },
            .handler = &runFold,
        });
        f4cf::devbench::addAction({
            .name = "answer",
            .description = "press a button of the dialog that asks what to do with the selected mod's unsaved changes before it is left or the menu is closed: "
                           "save writes them and then does what was asked, discard drops them and does it, cancel stays; Save is not pressed while a change is invalid. "
                           "An MCM mod's dialog has Keep for save and Revert for discard",
            .arguments = { { "button", { { "type", "string" }, { "enum", { "save", "discard", "cancel" } }, { "description", "answer: the dialog's button" } } } },
            .handler = &runAnswer,
        });
        f4cf::devbench::addAction({
            .name = "help",
            .description = "show the Help page over what is selected, as the title bar's Help does, or the selection's page again",
            .arguments = { { "shown", { { "type", "boolean" }, { "description", "help: false shows the selection's page again; true by default" } } } },
            .handler = &runHelp,
        });
        f4cf::devbench::addAction({
            .name = "minimize",
            .description = "hide the panel, as Minimize does, or show it again in front of the player, as Restore does",
            .arguments = { { "minimized", { { "type", "boolean" }, { "description", "minimize: false shows the panel again; true by default" } } } },
            .handler = &runMinimize,
        });
    }
}
