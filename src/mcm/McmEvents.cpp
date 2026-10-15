#include "McmStore.h"

#include "f4sevr/PapyrusUtils.h"

using namespace mods_config;

namespace
{
    /**
     * An event that is sent, for the function F4SE calls with each script that registered for it.
     */
    struct SentEvent
    {
        F4SEVR::VirtualMachine* vm;
        const std::vector<std::string>& arguments;
        int scripts = 0;
    };

    /**
     * Calls the function a script registered for the event, with the event's texts. The call is queued to
     * Papyrus, which runs it on its own time. A script that is gone since it registered is not called.
     */
    void callScript(const std::uint64_t handle, const char* scriptName, const char* callbackName, void* data)
    {
        auto& sent = *static_cast<SentEvent*>(data);

        F4SEVR::VMValue receiver;
        const F4SEVR::BSFixedString script(scriptName);
        if (!F4SEVR::GetIdentifier(&receiver, handle, &script, sent.vm) || !receiver.IsIdentifier() || !receiver.data.id) {
            return;
        }

        F4SEVR::VMArray<F4SEVR::VMVariable> arguments;
        for (const auto& argument : sent.arguments) {
            F4SEVR::addArgument(arguments, F4SEVR::BSFixedString(argument.c_str()));
        }
        F4SEVR::VMValue packed;
        F4SEVR::PackValue(&packed, &arguments, sent.vm);
        const F4SEVR::BSFixedString callback(callbackName);
        F4SEVR::CallFunctionNoWait_Internal(sent.vm, 0, receiver.data.id, &callback, &packed);
        ++sent.scripts;
    }
}

namespace mods_config::mcm
{
    /**
     * Sends an event to every script that registered for it with F4SE's `RegisterForExternalEvent`, as F4SE
     * does for MCM's menu: F4SE keeps the scripts of an event by its name, and each is called with the
     * function it registered.
     */
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments)
    {
        const auto* papyrus = F4SE::GetPapyrusInterface();
        const auto* gameVM = F4SEVR::getGameVM();
        if (!papyrus || !gameVM || !gameVM->m_virtualMachine) {
            logger::warn("The event {} is not sent: Papyrus is not running", event);
            return;
        }
        SentEvent sent{ .vm = gameVM->m_virtualMachine, .arguments = arguments };
        papyrus->GetExternalEventRegistrations(event, &sent, callScript);
        logger::debug("Sent the event {} to {} scripts", event, sent.scripts);
    }

    /**
     * Sends the game's `MenuOpenCloseEvent` of `PauseMenu`, opening and then closing, to every listener of the
     * UI's event, as the game sends it: the pause menu itself is not opened. Both are sent, so a listener that
     * counts the menus that are open stays right. It is called on the game's thread, as the game sends it.
     */
    void sendPauseMenuEvents()
    {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            logger::warn("The pause menu's events are not sent: the game's UI is not running");
            return;
        }
        auto* source = ui->GetEventSource<RE::MenuOpenCloseEvent>();
        source->Notify(RE::MenuOpenCloseEvent{ .menuName = "PauseMenu", .opening = true });
        source->Notify(RE::MenuOpenCloseEvent{ .menuName = "PauseMenu", .opening = false });
        logger::info("Sent the pause menu as opened and closed, for a DLL that reads its MCM file then");
    }
}
