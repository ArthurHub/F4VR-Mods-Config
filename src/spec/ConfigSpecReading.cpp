#include "ConfigSpecReading.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

#include "common/CommonUtils.h"
#include "model/TextMarkup.h"

namespace mods_config::spec
{
    namespace
    {
        /**
         * How many single characters have to change to make one name the other, without regard to case.
         */
        std::size_t editDistance(const std::string_view from, const std::string_view to)
        {
            std::vector<std::size_t> row(to.size() + 1);
            for (std::size_t j = 0; j < row.size(); ++j) {
                row[j] = j;
            }
            for (std::size_t i = 1; i <= from.size(); ++i) {
                std::size_t diagonal = row[0];
                row[0] = i;
                for (std::size_t j = 1; j <= to.size(); ++j) {
                    const std::size_t above = row[j];
                    const bool same = std::tolower(static_cast<unsigned char>(from[i - 1])) == std::tolower(static_cast<unsigned char>(to[j - 1]));
                    row[j] = (std::min)({ row[j] + 1, row[j - 1] + 1, diagonal + (same ? 0 : 1) });
                    diagonal = above;
                }
            }
            return row[to.size()];
        }

        // a text the player sees, written as an object
        constexpr std::array<std::string_view, 2> TEXT_FIELDS{ "token", "fallback" };
    }

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * Whether a key is true. False for a value with no such key, or with one that holds something else than a bool.
     */
    bool getBool(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_boolean() && found->get<bool>();
    }

    /**
     * A number of a key, or else the given one.
     */
    float getNumber(const Json& value, const char* key, const float otherwise)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number() ? found->get<float>() : otherwise;
    }

    /**
     * The shared list of a config spec by its name, which a choice can name in another case. None for a name of
     * no list.
     */
    const Json* findSharedList(const Json* sharedLists, const std::string& name)
    {
        if (!sharedLists) {
            return nullptr;
        }
        const auto lower = common::str_tolower(name);
        for (const auto& [listName, list] : sharedLists->items()) {
            if (common::str_tolower(listName) == lower) {
                return &list;
            }
        }
        return nullptr;
    }

    /**
     * A text of a key that the player sees, in the game's language. It is plain text, a `$` token, or a token
     * with the text that is shown where the language has none, as `{ "token": "$MyMod_Height", "fallback":
     * "Height" }`. Empty for a key that is none of them.
     */
    std::string getShownText(const Json& value, const char* key, const model::Translations& translations)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return {};
        }
        if (found->is_string()) {
            return translations.translate(found->get<std::string>());
        }
        return found->is_object() ? translations.translate(getText(*found, "token"), getText(*found, "fallback")) : std::string();
    }

    /**
     * A text the player sees, where the reading is, as `getShownText` reads one. A token that the language has
     * no text for, and that has no fallback, is shown as it is written, and is reported.
     */
    std::string readShownText(const Json& text, ConfigSpecReading& reading)
    {
        if (!text.is_string() && !text.is_object()) {
            return {};
        }
        checkFields(text, TEXT_FIELDS, reading);
        const auto token = text.is_string() ? text.get<std::string>() : getText(text, "token");
        const auto fallback = text.is_object() ? getText(text, "fallback") : std::string();
        if (token.starts_with('$') && fallback.empty() && !reading.translations.has(token)) {
            reading.report(model::Severity::Warning, std::format("The token {} has no translation and no fallback: it is shown as it is written.", token));
        }
        return reading.translations.translate(token, fallback);
    }

    /**
     * A text of a key that the player sees, which `readShownText` reads. Empty for a key that has none.
     */
    std::string readShownText(const Json& value, const char* key, ConfigSpecReading& reading)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return {};
        }
        const Into into(reading, key);
        return readShownText(*found, reading);
    }

    /**
     * Reports each field of an object that is none of the ones it can have, which is not read: a typo, or a
     * field of a newer schema. A field near one it can have names it, as `Did you mean "visibleWhen"?`. A row the loader
     * made is not its author's: what its author wrote of it is checked as the row is made.
     */
    void checkFields(const Json& object, const std::span<const std::string_view> known, ConfigSpecReading& reading)
    {
        if (!object.is_object() || object.contains(MADE_ROW_PATH)) {
            return;
        }
        for (const auto& [key, value] : object.items()) {
            if (std::ranges::find(known, key) != known.end()) {
                continue;
            }
            const auto nearest = (std::ranges::min)(known, {}, [&key](const std::string_view field) {
                return editDistance(key, field);
            });
            const auto distance = editDistance(key, nearest);
            auto message = std::format("Unknown field \"{}\": it is not read.", key);
            if (distance <= 2 && distance * 2 < key.size()) {
                message += std::format(" Did you mean \"{}\"?", nearest);
            }
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::move(message));
        }
    }

    /**
     * The `applies` of a config spec, a page, a section or a setting. None for one that has none, and for one
     * that is not `unknown`, `live`, `save` or `restart`.
     */
    std::optional<model::Applies> findApplies(const Json& value)
    {
        const auto text = getText(value, "applies");
        if (text == "unknown") {
            return model::Applies::Unknown;
        }
        if (text == "live") {
            return model::Applies::Live;
        }
        if (text == "save") {
            return model::Applies::Save;
        }
        return text == "restart" ? std::optional(model::Applies::Restart) : std::nullopt;
    }

    /**
     * What a page, a section or a setting has for what is under it: its own `defaultFile`, `applies` and
     * `omitDefault`, or else what it gets from above. An `applies` or an `omitDefault` that is not one is passed
     * over, and is reported.
     */
    Inherited inherit(const Json& value, const Inherited& above, ConfigSpecReading& reading)
    {
        Inherited own = above;
        if (auto file = getText(value, "defaultFile"); !file.empty()) {
            own.defaultFile = std::move(file);
        }
        if (const auto applies = findApplies(value)) {
            own.applies = *applies;
        } else if (value.contains("applies")) {
            const Into into(reading, "applies");
            reading.report(model::Severity::Error, std::format("Its applies {} is not unknown, live, save or restart: it has the one above it.", value["applies"].dump()));
        }
        if (const auto omit = value.find("omitDefault"); omit != value.end()) {
            if (omit->is_boolean()) {
                own.omitDefault = omit->get<bool>();
            } else {
                const Into into(reading, "omitDefault");
                reading.report(model::Severity::Warning, "Its omitDefault is not true or false: it is left out.");
            }
        }
        return own;
    }

    /**
     * A row's translated label, without tags where `html` is true, for search and the help line.
     */
    std::string getLabel(const Json& row, ConfigSpecReading& reading)
    {
        auto text = readShownText(row, "text", reading);
        return getBool(row, "html") ? model::removeMarkup(text) : text;
    }

    /**
     * A label's styled pieces on one line where `html` is true. Other labels are drawn plain.
     */
    std::vector<model::TextRun> getLabelRuns(const Json& row, const model::Translations& translations)
    {
        return getBool(row, "html") ? model::markupRuns(getShownText(row, "text", translations)) : std::vector<model::TextRun>{};
    }
}
