#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "common/CommonUtils.h"
#include "model/ClientCalls.h"

namespace test_clients
{
    using namespace mods_config;

    /**
     * A mod's client for the model's client calls, of the lambdas a test gives it: its functions, its states and its
     * options of choices, each by its name, and its handler of the menu's events.
     */
    struct TestClient
    {
        std::map<std::string, std::function<model::FunctionResult(const std::vector<model::NamedArgument>& arguments)>> functions;
        std::map<std::string, std::function<std::optional<model::Value>()>> states;
        std::map<std::string, std::function<std::vector<model::Option>(const model::Setting& setting)>> options;
        std::function<void(model::ClientEvent event, std::string_view pageId)> onEvent;

        /**
         * The client the client calls ask, which finds each by its name with no regard to case, as the menu asks a
         * client: a choice whose options it does not have has none, with the reason.
         */
        model::ClientCalls::Client calls() const
        {
            const auto find = [](const auto& map, const std::string_view name) {
                return std::ranges::find_if(map, [name](const auto& each) {
                    return common::str_tolower(each.first) == common::str_tolower(std::string(name));
                });
            };
            return { .hasFunction =
                         [functions = functions, find](const std::string_view name) {
                             return find(functions, name) != functions.end();
                         },
                .runFunction =
                    [functions = functions, find](const std::string_view name, const std::vector<model::NamedArgument>& arguments) {
                        const auto found = find(functions, name);
                        return found != functions.end() ? found->second(arguments) : model::FunctionResult{};
                    },
                .state =
                    [states = states, find](const std::string_view name) {
                        const auto found = find(states, name);
                        return found != states.end() ? found->second() : std::nullopt;
                    },
                .options = [options = options, find](const model::Setting& setting) -> std::expected<std::vector<model::Option>, std::string> {
                    const auto found = find(options, setting.id);
                    if (found == options.end()) {
                        return std::unexpected("Not available: its mod's client gives no options for it.");
                    }
                    return found->second(setting);
                },
                .onEvent = onEvent };
        }
    };
}
