#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "ConfigDefinition.h"
#include "devbench/DevBench.h"

namespace mods_config::model
{
    /**
     * Publishes a Save of a mod's editing session as a devbench event, `session.saved`: the mod, whether it wrote
     * every change, and why not, as its store returns it.
     */
    inline void publishSaved(const std::string_view modName, const std::expected<void, std::string>& saved)
    {
        f4cf::devbench::emit("session.saved", [&] {
            return nlohmann::json{ { "mod", modName }, { "saved", saved.has_value() }, { "reason", saved ? nlohmann::json(nullptr) : nlohmann::json(saved.error()) } };
        });
    }

    /**
     * Publishes a Discard of a mod's editing session as a devbench event, `session.discarded`: the mod, and the setting of
     * a row's Discard, which is none for the footer's.
     */
    inline void publishDiscarded(const std::string_view modName, const Setting* setting = nullptr)
    {
        f4cf::devbench::emit("session.discarded", [&] {
            return nlohmann::json{ { "mod", modName }, { "setting", setting ? nlohmann::json(setting->id) : nlohmann::json(nullptr) } };
        });
    }
}
