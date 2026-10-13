#include "McmStore.h"

#include <type_traits>
#include <variant>

#include "f4sevr/PapyrusUtils.h"

using namespace mods_config;

namespace
{
    /**
     * The script of a form that a function is called on, as `mcm_vr.dll` finds it: the one that is named, or
     * the form's first one. It holds the script while it lives, and is none for a form with no such script.
     */
    class FormScript
    {
    public:
        FormScript(F4SEVR::VirtualMachine& vm, RE::TESForm& form, const std::string& script)
        {
            const auto handle = vm.GetHandlePolicy()->Create(static_cast<F4SEVR::UInt32>(form.GetFormType()), &form);
            vm.GetObjectIdentifier(handle, script.empty() ? "ScriptObject" : script.c_str(), 1, &_script, 0);
        }

        ~FormScript()
        {
            if (_script && !_script->DecrementLock()) {
                _script->Destroy();
            }
        }

        FormScript(const FormScript&) = delete;
        FormScript& operator=(const FormScript&) = delete;

        F4SEVR::VMIdentifier* get() const
        {
            return _script;
        }

    private:
        F4SEVR::VMIdentifier* _script = nullptr;
    };

    /**
     * Adds an argument of a call, in the type Papyrus gets it in: a bool, a whole number, a number or a text.
     */
    void addArgument(F4SEVR::VMArray<F4SEVR::VMVariable>& arguments, const model::Value& value)
    {
        std::visit(
            [&arguments]<typename T>(const T& typed) {
                if constexpr (std::is_same_v<T, int>) {
                    F4SEVR::addArgument(arguments, static_cast<F4SEVR::SInt32>(typed));
                } else if constexpr (std::is_same_v<T, std::string>) {
                    F4SEVR::addArgument(arguments, F4SEVR::BSFixedString(typed.c_str()));
                } else {
                    F4SEVR::addArgument(arguments, typed);
                }
            },
            value);
    }

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
}

namespace mods_config::mcm
{
    /**
     * Calls a function of a script of a form, as `mcm_vr.dll` does for MCM's menu. The call is queued to
     * Papyrus, which runs it on its own time and gives no result: a script that has no such function, or
     * one with other parameters, has its error in Papyrus's log alone.
     * Returns false for a call that is not queued, with the reason in the log: the form is not in the game,
     * or it has no such script attached.
     */
    bool callFunction(const model::CallFunction& call)
    {
        const auto* gameVM = F4SEVR::getGameVM();
        auto* vm = gameVM ? gameVM->m_virtualMachine : nullptr;
        if (!vm) {
            logger::warn("The function {} is not called: Papyrus is not running", call.function);
            return false;
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

        F4SEVR::VMArray<F4SEVR::VMVariable> arguments;
        for (const auto& argument : call.arguments) {
            addArgument(arguments, argument);
        }
        F4SEVR::VMValue packed;
        F4SEVR::PackValue(&packed, &arguments, vm);
        const F4SEVR::BSFixedString function(call.function.c_str());
        F4SEVR::CallFunctionNoWait_Internal(vm, 0, script.get(), &function, &packed);
        logger::debug("Called the function {}({}) of the form {:X} of '{}'", call.function, describeArguments(call.arguments), call.formId, call.plugin);
        return true;
    }
}
