#pragma once

#include <expected>
#include <memory>
#include <string>
#include <vector>

#include "ModsConfigVRApi.h"
#include "model/ChoiceOptions.h"
#include "model/Translations.h"

namespace mods_config::api
{
    /**
     * The host's side of a client: its name, the version of its header, and whether it is cut off.
     */
    struct Client
    {
        std::string name;
        std::uint32_t version = 0;
        void* ctx = nullptr;

        // a call of it failed, so nothing of it is called again until the game is restarted
        bool cutOff = false;
    };

    /**
     * The native clients of the mods, which register through the API, each under the name of its mod. What a client
     * registers is its config spec's: the callbacks of its options go into the client options the config spec store
     * reads. Every call of a client is in a guard: one that throws or faults is cut off, and its mod is handled as
     * one with no client. It knows nothing of the game, so a test runs it.
     */
    class NativeClients
    {
    public:
        explicit NativeClients(model::ClientOptions& options)
            : _options(options)
        {}

        // the texts of the game's language, which a client's texts are translated with
        void setTranslations(const model::Translations* translations)
        {
            _translations = translations;
        }

        Client* registerClient(const ClientInfo* info);
        bool registerOptions(Client* client, const char* settingId, OptionsFn options, void* ctx);

    private:
        std::expected<std::vector<model::Option>, std::string> callOptions(Client& client, OptionsFn options, void* ctx, const model::Setting& setting) const;

        model::ClientOptions& _options;
        const model::Translations* _translations = nullptr;

        // each kept where it is, as a client keeps its handle
        std::vector<std::unique_ptr<Client>> _clients;
    };
}
