#include "GameActions.h"

#include <type_traits>
#include <variant>

#include <Windows.h>
#include <shellapi.h>

#include "api/ModsConfigFO4Api.h"

using namespace mods_config;

namespace
{
    /**
     * The script of a form that a function is called on, as MCM's DLL finds it: the one that is named, or
     * the form's first one. It holds the script while it lives, and is none for a form with no such script.
     */
    class FormScript
    {
    public:
        FormScript(RE::BSScript::IVirtualMachine& vm, RE::TESForm& form, const std::string& script)
        {
            const auto handle = vm.GetObjectHandlePolicy().GetHandleForObject(static_cast<std::uint32_t>(form.GetFormType()), &form);
            vm.FindBoundObject(handle, script.empty() ? "ScriptObject" : script.c_str(), true, _script, false);
        }

        const RE::BSTSmartPointer<RE::BSScript::Object>& get() const
        {
            return _script;
        }

    private:
        RE::BSTSmartPointer<RE::BSScript::Object> _script;
    };

    /**
     * The arguments of a call for the log, each so that its type shows: `true`, `4`, `4.0` and `"a text"`.
     */
    std::string describeArguments(const std::vector<model::Value>& arguments)
    {
        std::string described;
        for (const auto& argument : arguments) {
            described += described.empty() ? "" : ", ";
            if (const auto* on = std::get_if<bool>(&argument)) {
                described += *on ? "true" : "false";
            } else if (const auto* whole = std::get_if<int>(&argument)) {
                described += std::format("{}", *whole);
            } else if (const auto* number = std::get_if<float>(&argument)) {
                // a number that is written whole gets its point, to tell it from a whole number
                const auto text = std::format("{}", *number);
                described += text.find_first_not_of("-0123456789") == std::string::npos ? text + ".0" : text;
            } else {
                described += std::format("\"{}\"", std::get<std::string>(argument));
            }
        }
        return described;
    }

    /**
     * The arguments of a call, packed as Papyrus takes them: a bool, a whole number, a number or a text.
     */
    RE::BSScrapArray<RE::BSScript::Variable> packArguments(const std::vector<model::Value>& values)
    {
        RE::BSScrapArray<RE::BSScript::Variable> packed{ static_cast<std::uint32_t>(values.size()) };
        for (std::size_t i = 0; i < values.size(); ++i) {
            std::visit(
                [&packed, i](const auto& typed) {
                    RE::BSScript::PackVariable(packed[static_cast<std::uint32_t>(i)], typed);
                },
                values[i]);
        }
        return packed;
    }
}

namespace mods_config
{
    /**
     * Calls a function of a script of a form, or a global function of a script, as MCM's DLL does for
     * MCM's menu. The call is queued to Papyrus, which runs it on its own time and gives no result: a script
     * that has no such function, or one with other parameters, has its error in Papyrus's log alone, and so
     * has a global function's script that is not in the game, which is not looked up.
     * Returns false for a call that is not queued, with the reason in the log: the form is not in the game,
     * or it has no such script attached.
     */
    bool callFunction(const model::CallFunction& call)
    {
        const auto gameVM = RE::GameVM::GetSingleton();
        const auto vm = gameVM ? gameVM->GetVM() : nullptr;
        if (!vm) {
            logger::warn("The function {} is not called: Papyrus is not running", call.function);
            return false;
        }
        if (call.plugin.empty()) {
            if (!vm->DispatchStaticCallPacked(call.script.c_str(), call.function.c_str(), nullptr, packArguments(call.arguments))) {
                logger::warn("The global function {}.{} is not called: Papyrus did not take the call", call.script, call.function);
                return false;
            }
            logger::debug("Called the global function {}.{}({})", call.script, call.function, describeArguments(call.arguments));
            return true;
        }

        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        auto* form = dataHandler ? dataHandler->LookupForm(call.formId, call.plugin) : nullptr;
        if (!form) {
            logger::warn("The function {} is not called: the form {:X} of '{}' is not in the game", call.function, call.formId, call.plugin);
            return false;
        }
        const FormScript script(*vm, *form, call.script);
        if (!script.get()) {
            logger::warn("The function {} is not called: the form {:X} of '{}' has no script '{}' attached", call.function, call.formId, call.plugin, call.script);
            return false;
        }
        if (!vm->DispatchMethodCallPacked(script.get(), call.function.c_str(), nullptr, packArguments(call.arguments))) {
            logger::warn("The function {} is not called: Papyrus did not take the call on the form {:X} of '{}'", call.function, call.formId, call.plugin);
            return false;
        }
        logger::debug("Called the function {}({}) of the form {:X} of '{}'", call.function, describeArguments(call.arguments), call.formId, call.plugin);
        return true;
    }

    /**
     * Runs a line of the console as the game's console runs what the player types, as MCM's DLL does for
     * the RunConsoleCommand of a hotkey. It runs at once, on the game thread, and gives no result: a line the
     * console does not know has its error in the console alone.
     */
    bool runConsoleCommand(const model::ConsoleCommand& command)
    {
        RE::Console::ExecuteCommand(command.command.c_str());
        logger::debug("Ran the console command '{}'", command.command);
        return true;
    }

    /**
     * Opens a link with Windows' default handler, using UTF-16 so non-ASCII URLs are preserved.
     * A failed launch is logged and returned to the button's result.
     */
    bool openWebLink(const model::WebLink& link)
    {
        const auto url = F4SE::stl::utf8_to_utf16(link.url);
        if (!url) {
            logger::warn("The web link is not opened: its URL is not UTF-8");
            return false;
        }
        const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", url->c_str(), nullptr, SW_SHOWNORMAL));
        if (result <= 32) {
            logger::warn("The web link '{}' is not opened: Windows returned {}", link.url, result);
            return false;
        }
        logger::debug("Opened the web link '{}' in the desktop browser", link.url);
        return true;
    }

    /**
     * Sends the F4SE message of a Save, `MODS_CONFIG_FO4_SAVED_MESSAGE` of the client header, to each plugin, with the
     * mod's name as its data, text that ends with a zero. F4SE calls the plugin's listener of the menu's messages at
     * once, and a plugin with none gets nothing: the log says what F4SE answered, at debug, since most plugins have no
     * listener. It calls F4SE's own interface, as CommonLibF4's Dispatch warns of each message nobody takes.
     */
    void sendSavedMessage(const std::string& modName, const std::vector<std::string>& plugins)
    {
        const auto* messaging = F4SE::GetMessagingInterface();
        if (!messaging) {
            return;
        }
        const auto& f4se = reinterpret_cast<const F4SE::detail::F4SEMessagingInterface&>(*messaging);
        for (const auto& plugin : plugins) {
            const bool sent = f4se.Dispatch(F4SE::GetPluginHandle(),
                api::MODS_CONFIG_FO4_SAVED_MESSAGE,
                const_cast<char*>(modName.c_str()),
                static_cast<std::uint32_t>(modName.size() + 1),
                plugin.c_str());
            logger::debug("The F4SE message of the Save of '{}' {} sent to '{}'", modName, sent ? "is" : "is not", plugin);
        }
    }
}
