#include "McmStore.h"

#include "devbench/DevBench.h"

using namespace mods_config;

namespace
{
    /**
     * An event that is sent, for the function F4SE calls with each script that registered for it.
     */
    struct SentEvent
    {
        RE::BSScript::IVirtualMachine* vm;
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

        RE::BSScrapArray<RE::BSScript::Variable> arguments{ static_cast<std::uint32_t>(sent.arguments.size()) };
        for (std::uint32_t i = 0; i < arguments.size(); ++i) {
            RE::BSScript::PackVariable(arguments[i], sent.arguments[i]);
        }
        // a script that is gone has no object bound to the handle, which the VM does not call
        if (sent.vm->DispatchMethodCallPacked(handle, scriptName, callbackName, nullptr, std::move(arguments))) {
            ++sent.scripts;
        }
    }
}

namespace mods_config::mcm
{
    /**
     * Sends an event to every script that registered for it with F4SE's `RegisterForExternalEvent`, as F4SE
     * does for MCM's menu: F4SE keeps the scripts of an event by its name, and each is called with the
     * function it registered. It is published as the devbench event `mcm.event`, with how many scripts it was sent to.
     */
    void sendEvent(const std::string& event, const std::vector<std::string>& arguments)
    {
        const auto* papyrus = F4SE::GetPapyrusInterface();
        const auto gameVM = RE::GameVM::GetSingleton();
        const auto vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!papyrus || !vm) {
            logger::warn("The event {} is not sent: Papyrus is not running", event);
            return;
        }
        SentEvent sent{ .vm = vm.get(), .arguments = arguments };
        papyrus->GetExternalEventRegistrations(event, &sent, callScript);
        logger::debug("Sent the event {} to {} scripts", event, sent.scripts);
        f4cf::devbench::emit("mcm.event", [&] {
            return nlohmann::json{ { "event", event }, { "arguments", arguments }, { "scripts", sent.scripts } };
        });
    }

    /**
     * Sends the game's `MenuOpenCloseEvent` of `PauseMenu`, opening and then closing, to every listener of the
     * UI's event, as the game sends it: the pause menu itself is not opened. Both are sent, so a listener that
     * counts the menus that are open stays right. It is called on the game's thread, as the game sends it. It is
     * published as the devbench event `mcm.pauseMenu`.
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
        f4cf::devbench::emit("mcm.pauseMenu");
    }
}
