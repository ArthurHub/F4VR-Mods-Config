#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ModsConfigVRApi.h"
#include "model/ClientCalls.h"
#include "model/Translations.h"

namespace mods_config::api
{
    /**
     * The native clients of the mods, which register through the API, each under the name of its mod. A client is
     * added to the client calls the config spec store reads, which ask it by name for its functions, states and
     * options, and tell it the menu's events, the changes of its `live` settings and what a Save wrote. Every call of a client is in a guard: one that throws or faults is cut
     * off, and its mod is handled as one with no client. It knows nothing of the game, so a test runs it.
     */
    class NativeClients
    {
    public:
        explicit NativeClients(model::ClientCalls& calls)
            : _calls(calls)
        {}

        // the texts of the game's language, which a client's texts are translated with
        void setTranslations(const model::Translations* translations)
        {
            _translations = translations;
        }

        bool registerClient(const char* name, std::uint32_t version, IClient* client);
        bool addQuickLaunch(const IClient* client, const char* text, const char* help, const char* buttonText, const char* functionName);
        const std::string* nameOf(const IClient* client) const;

    private:
        /**
         * The host's side of a client: its name, the version of its header, and whether it is cut off.
         */
        struct Registered
        {
            std::string name;
            std::uint32_t version = 0;
            IClient* client = nullptr;

            // a call of it failed, so nothing of it is called again until the game is restarted
            bool cutOff = false;
        };

        bool hasFunction(Registered& registered, std::string_view name) const;
        model::FunctionResult runFunction(Registered& registered, std::string_view name, const std::vector<model::NamedArgument>& arguments) const;
        std::optional<model::Value> answerState(Registered& registered, std::string_view name) const;
        std::expected<std::vector<model::Option>, std::string> giveOptions(Registered& registered, const model::Setting& setting) const;
        void tellEvent(Registered& registered, model::ClientEvent event, std::string_view pageId) const;
        void tellChange(Registered& registered, const model::SettingChange& change) const;
        void tellSave(Registered& registered, const model::SaveBatch& batch) const;
        template <typename F>
        bool call(Registered& registered, std::string_view what, F& function) const;
        const Registered* find(const IClient* client) const;

        model::ClientCalls& _calls;
        const model::Translations* _translations = nullptr;

        // each kept where it is, as its calls point to it
        std::vector<std::unique_ptr<Registered>> _clients;
    };
}
