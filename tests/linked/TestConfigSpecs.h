#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "model/OptionSets.h"
#include "spec/ConfigSpecLoader.h"

// The config specs, the reading and the checks the tests of the config spec loader and its parts share.
namespace test_config_specs
{
    using namespace mods_config;

    using Lines = std::vector<std::string>;

    /**
     * A config spec that has to load.
     */
    inline model::Mod loaded(const std::string_view text, const model::Translations& translations = {})
    {
        auto mod = spec::loadConfigSpec(text, translations);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A config spec with the given content as its front page, a file `main` that is its default, and the given
     * root `applies`.
     */
    inline std::string configSpecOf(const std::string& content, const std::string& applies = "live")
    {
        return std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "{}",
            "files": {{ "main": {{ "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" }} }},
            "defaultFile": "main",
            "content": [ {} ]
        }})",
            applies,
            content);
    }

    /**
     * Every setting of a page and of the pages under it, in order, with those of its sections.
     */
    inline void forEachSetting(const model::Page& page, const std::function<void(const model::Setting&)>& visit)
    {
        const std::function<void(const std::vector<model::Item>&)> inItems = [&](const std::vector<model::Item>& items) {
            for (const auto& item : items) {
                if (const auto* setting = std::get_if<model::Setting>(&item)) {
                    visit(*setting);
                } else if (const auto* group = std::get_if<model::Group>(&item)) {
                    inItems(group->items);
                }
            }
        };
        inItems(page.items);
        for (const auto& under : page.pages) {
            forEachSetting(under, visit);
        }
    }

    /**
     * The settings of a mod by their ids, in order.
     */
    inline std::vector<model::Setting> settingsOf(const model::Mod& mod)
    {
        std::vector<model::Setting> settings;
        const auto add = [&settings](const model::Setting& setting) {
            settings.push_back(setting);
        };
        forEachSetting(mod.frontPage, add);
        for (const auto& page : mod.pages) {
            forEachSetting(page, add);
        }
        return settings;
    }

    inline model::Setting onlySetting(const model::Mod& mod)
    {
        const auto settings = settingsOf(mod);
        REQUIRE(settings.size() == 1);
        return settings[0];
    }

    inline model::Storage fileValue(const std::string& file, Lines path)
    {
        return model::FileStorage{ .file = file, .path = std::move(path) };
    }

    /**
     * A problem as one line: E or W, its row, its message, where it is, and its path.
     */
    inline std::string problemLine(const model::Problem& problem)
    {
        return std::format("{} {}: {} [{}] {}", problem.severity == model::Severity::Error ? "E" : "W", problem.row, problem.message, problem.where, problem.path);
    }

    inline Lines problemLines(const model::Mod& mod)
    {
        Lines lines;
        for (const auto& problem : mod.problems) {
            lines.push_back(problemLine(problem));
        }
        return lines;
    }

    // a config spec of one TOML file, of which `rows` is the content
    inline std::string sphereSpec(const std::string& rows)
    {
        return std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod",
            "files": {{
                "main": {{ "format": "toml", "root": "myGames", "path": "MyMod.toml" }},
                "ini": {{ "format": "ini", "root": "myGames", "path": "MyMod.ini" }}
            }},
            "defaultFile": "main",
            "content": [ {} ]
        }})",
            rows);
    }

    /**
     * The value an option of a choice sets on a setting, by its id, and none where it sets none.
     */
    inline std::optional<model::Value> setOn(const model::Option& option, const std::string& id)
    {
        const auto found = std::ranges::find(option.sets, id, &model::OptionSet::setting);
        return found != option.sets.end() ? std::optional(found->value) : std::nullopt;
    }
}
