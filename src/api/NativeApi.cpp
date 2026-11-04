#include "ModsConfigFO4.h"
#include "ModsConfigFO4Api.h"

using namespace mods_config;

namespace
{
    /**
     * Mods Config FO4 VR's side of the native API, which the menu and its native clients answer. A call of the menu is
     * asked of it, which does it in its frame update.
     */
    class HostApi final : public api::ModsConfigFO4ApiV1
    {
    public:
        std::uint32_t getVersion() const override
        {
            return api::MODS_CONFIG_FO4_API_VERSION;
        }

        const char* getModVersion() const override
        {
            return Version::NAME.data();
        }

        bool addQuickLaunch(api::IClient* client, const char* text, const char* help, const char* buttonText, const char* functionName) const override
        {
            return g_modsConfig.menu().nativeClients().addQuickLaunch(client, text, help, buttonText, functionName);
        }

        bool registerClient(const char* name, const std::uint32_t version, api::IClient* client) const override
        {
            return g_modsConfig.menu().nativeClients().registerClient(name, version, client);
        }

        api::MenuState getMenuState() const override
        {
            const auto& menu = g_modsConfig.menu();
            return !menu.isOpen() ? api::MenuState::Closed : menu.isMinimized() ? api::MenuState::Minimized : api::MenuState::Open;
        }

        bool openMenu(const char* modName, const char* pageId) const override
        {
            return g_modsConfig.menu().requestOpen(modName ? modName : "", pageId ? pageId : "");
        }

        void minimizeMenu() const override
        {
            g_modsConfig.menu().requestMinimize();
        }

        void restoreMenu() const override
        {
            g_modsConfig.menu().requestRestore();
        }

        void reloadValues(api::IClient* client) const override
        {
            auto& menu = g_modsConfig.menu();
            if (const auto* name = menu.nativeClients().nameOf(client)) {
                menu.requestReadAgain(*name);
            }
        }

        bool setPendingValue(api::IClient* client, const char* settingId, const api::SettingValue* value) const override
        {
            return g_modsConfig.menu().nativeClients().setPendingValue(client, settingId, value);
        }
    };

    // the API, which is the same for the whole run of the game
    const HostApi API;
}

/**
 * The native API, which a client's header gets by this name with `GetProcAddress`.
 */
extern "C" DLLEXPORT const api::ModsConfigFO4ApiV1* MODS_CONFIG_FO4_CALL ModsConfigFO4API_GetApiV1()
{
    return &API;
}
