#include "ModsConfigVR.h"
#include "ModsConfigVRApi.h"

using namespace mods_config;

namespace
{
    std::uint32_t MODS_CONFIG_VR_CALL getVersion()
    {
        return api::MODS_CONFIG_VR_API_VERSION;
    }

    const char* MODS_CONFIG_VR_CALL getModVersion()
    {
        return Version::NAME.data();
    }

    api::Client* MODS_CONFIG_VR_CALL registerClient(const api::ClientInfo* info)
    {
        return g_modsConfig.menu().nativeClients().registerClient(info);
    }

    bool MODS_CONFIG_VR_CALL registerOptions(api::Client* client, const char* settingId, const api::OptionsFn options, void* ctx)
    {
        return g_modsConfig.menu().nativeClients().registerOptions(client, settingId, options, ctx);
    }

    bool MODS_CONFIG_VR_CALL registerFunction(api::Client* client, const char* name, const api::NativeFunction function, void* ctx)
    {
        return g_modsConfig.menu().nativeClients().registerFunction(client, name, function, ctx);
    }

    bool MODS_CONFIG_VR_CALL registerState(api::Client* client, const char* name, const api::StateFn state, void* ctx)
    {
        return g_modsConfig.menu().nativeClients().registerState(client, name, state, ctx);
    }

    api::MenuState MODS_CONFIG_VR_CALL getMenuState()
    {
        const auto& menu = g_modsConfig.menu();
        return !menu.isOpen() ? api::MenuState::Closed : menu.isMinimized() ? api::MenuState::Minimized : api::MenuState::Open;
    }

    bool MODS_CONFIG_VR_CALL openMenu(const char* modName, const char* pageId)
    {
        return g_modsConfig.menu().requestOpen(modName ? modName : "", pageId ? pageId : "");
    }

    void MODS_CONFIG_VR_CALL minimizeMenu()
    {
        g_modsConfig.menu().requestMinimize();
    }

    void MODS_CONFIG_VR_CALL restoreMenu()
    {
        g_modsConfig.menu().requestRestore();
    }

    void MODS_CONFIG_VR_CALL reloadValues(api::Client* client)
    {
        auto& menu = g_modsConfig.menu();
        if (menu.nativeClients().isClient(client)) {
            menu.requestReadAgain(client->name);
        }
    }

    // the API, which is the same for the whole run of the game
    constexpr api::ModsConfigVRApiV1 API{
        .getVersion = &getVersion,
        .getModVersion = &getModVersion,
        .registerClient = &registerClient,
        .registerOptions = &registerOptions,
        .registerFunction = &registerFunction,
        .registerState = &registerState,
        .getMenuState = &getMenuState,
        .openMenu = &openMenu,
        .minimizeMenu = &minimizeMenu,
        .restoreMenu = &restoreMenu,
        .reloadValues = &reloadValues,
    };
}

/**
 * The native API, which a client's header gets by this name with `GetProcAddress`.
 */
extern "C" DLLEXPORT const api::ModsConfigVRApiV1* MODS_CONFIG_VR_CALL ModsConfigVRAPI_GetApiV1()
{
    return &API;
}
