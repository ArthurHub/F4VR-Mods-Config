#include "RequirementSpec.h"

#include <format>

namespace mods_config::spec
{
    namespace
    {
        void ignoreMessage(F4SE::MessagingInterface::Message*)
        {}

        /**
         * Names as a reason says them: `A`, `A and B`, `A, B and C`.
         */
        std::string joinNames(const std::vector<std::string>& names)
        {
            std::string joined;
            for (std::size_t i = 0; i < names.size(); ++i) {
                joined += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
            }
            return joined;
        }

        /**
         * The plugins of a list of a config spec's requirements that are not loaded, in its order. None with no check.
         */
        std::vector<std::string> findMissing(const Json& configSpec, const char* key, const spec::PluginCheck& isLoaded)
        {
            std::vector<std::string> missing;
            if (!isLoaded) {
                return missing;
            }
            for (auto& plugin : requiredPlugins(configSpec, key)) {
                if (!isLoaded(plugin)) {
                    missing.push_back(std::move(plugin));
                }
            }
            return missing;
        }
    }

    /**
     * Whether a game plugin is loaded in the running game: one that is in the Data folder and not enabled is not.
     */
    bool isGamePluginLoaded(const std::string_view plugin)
    {
        auto* const dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupLoadedFile(plugin).first;
    }

    /**
     * Whether an F4SE plugin is loaded, by the name it registers with F4SE. F4SE VR has no call that asks it,
     * so the mod registers a listener of the plugin's messages, which F4SE refuses for a plugin that is not
     * loaded. The listener does nothing, and stays: F4SE keeps the first listener the mod registers for a
     * plugin, so the mod cannot listen to the messages of a plugin it asked of.
     */
    bool isF4sePluginLoaded(const std::string_view plugin)
    {
        const auto* messaging = F4SE::GetMessagingInterface();
        const std::string name(plugin);
        return messaging && messaging->RegisterListener(ignoreMessage, name);
    }

    /**
     * The plugins a list of a config spec's requirements names, in its order: none where it has no list, and an entry
     * that is no text is left out.
     */
    std::vector<std::string> requiredPlugins(const Json& configSpec, const char* key)
    {
        std::vector<std::string> plugins;
        const auto required = configSpec.find(key);
        if (required == configSpec.end() || !required->is_array()) {
            return plugins;
        }
        for (const auto& plugin : *required) {
            if (plugin.is_string()) {
                plugins.push_back(plugin.get<std::string>());
            }
        }
        return plugins;
    }

    /**
     * Why a config spec's mod is not available while a plugin it requires is not loaded: a game plugin of its
     * `pluginRequirements` or an F4SE plugin of its `f4sePluginRequirements`, as `its plugin MyMod.esp and
     * its F4SE plugin MyMod are not loaded`. Empty while every one is, and with no checks, as in a test of a
     * config spec alone.
     */
    std::string findMissingRequirements(const Json& configSpec, const spec::PluginChecks& plugins)
    {
        const auto game = findMissing(configSpec, "pluginRequirements", plugins.isGamePluginLoaded);
        const auto f4se = findMissing(configSpec, "f4sePluginRequirements", plugins.isF4sePluginLoaded);
        std::vector<std::string> parts;
        if (!game.empty()) {
            parts.push_back(std::format("its plugin{} {}", game.size() == 1 ? "" : "s", joinNames(game)));
        }
        if (!f4se.empty()) {
            parts.push_back(std::format("its F4SE plugin{} {}", f4se.size() == 1 ? "" : "s", joinNames(f4se)));
        }
        if (parts.empty()) {
            return {};
        }
        return std::format("{} {} not loaded", joinNames(parts), game.size() + f4se.size() == 1 ? "is" : "are");
    }

    /**
     * The `minSchemaVersion` of a config spec: a whole number from 1. None for a config spec that has none.
     */
    std::optional<std::int64_t> findSchemaVersion(const Json& configSpec)
    {
        const auto version = configSpec.find("minSchemaVersion");
        if (version == configSpec.end() || !version->is_number_integer() || version->get<std::int64_t>() < 1) {
            return std::nullopt;
        }
        return version->get<std::int64_t>();
    }
}
