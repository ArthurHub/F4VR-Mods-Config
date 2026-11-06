#include "GameValues.h"

#include <map>
#include <tuple>
#include <type_traits>
#include <variant>

#include "f4sevr/PapyrusUtils.h"

using namespace mods_config;

namespace
{
    /**
     * What is kept of a property that was looked up, so a page that asks for its value in every frame looks
     * up no name again.
     */
    struct LookedProperty
    {
        // none for a form that is not in the game: its plugin is not loaded, or has no form of that id
        RE::TESForm* form = nullptr;

        // the type of the script the place was looked up in, and the place of the property in it: -1 for a
        // script that has no property of that name
        const F4SEVR::VMObjectTypeInfo* type = nullptr;
        F4SEVR::SInt32 index = -1;

        // why it is not in the game, as the log has it last
        std::string warned;
    };

    /**
     * The property as it was looked up, with its form: the forms of a plugin are the same while the game
     * runs, so each is looked up once.
     */
    LookedProperty& lookedProperty(const model::PropertyStorage& property)
    {
        static std::map<std::tuple<std::string, std::uint32_t, std::string, std::string>, LookedProperty> looked;

        const auto [found, isNew] = looked.try_emplace({ property.plugin, property.formId, property.script, property.name });
        if (isNew) {
            const auto dataHandler = RE::TESDataHandler::GetSingleton();
            found->second.form = dataHandler ? dataHandler->LookupForm(property.formId, property.plugin) : nullptr;
        }
        return found->second;
    }

    /**
     * Says in the log why a property is not in the game, once for a reason: a page asks for it in every frame.
     */
    void warnOnce(const model::PropertyStorage& property, LookedProperty& looked, const char* reason)
    {
        if (looked.warned != reason) {
            looked.warned = reason;
            logger::warn("The property {} of the form {:X} of '{}' is not available: {}", property.name, property.formId, property.plugin, reason);
        }
    }

    /**
     * A property in the game: the script of its form that has it, and its place in the script, as
     * `mcm_vr.dll` finds them. The script is the one the property names, or the form's first one.
     * It holds the script while it lives. It is none for a property that is not in the game, which the log
     * has the reason of: its form is not there, no such script is attached to it, or the script has no
     * property of that name.
     */
    class ScriptProperty
    {
    public:
        explicit ScriptProperty(const model::PropertyStorage& property)
        {
            auto& looked = lookedProperty(property);
            if (const auto* reason = find(property, looked)) {
                warnOnce(property, looked, reason);
            }
        }

        ~ScriptProperty()
        {
            if (_script && !_script->DecrementLock()) {
                _script->Destroy();
            }
        }

        ScriptProperty(const ScriptProperty&) = delete;
        ScriptProperty& operator=(const ScriptProperty&) = delete;

        explicit operator bool() const
        {
            return _script && _index >= 0;
        }

        /**
         * The value the script has for the property now.
         */
        void get(F4SEVR::VMValue& value)
        {
            _vm->GetPropertyValueByIndex(&_script, _index, &value);
        }

        /**
         * Sets the property in the script, by its name. A property with a function of its own runs it.
         */
        void set(const char* name, F4SEVR::VMValue& value)
        {
            F4SEVR::UInt64 unknown = 0;
            _vm->SetPropertyValue(&_script, name, &value, &unknown);
        }

    private:
        /**
         * Finds the script and the place of the property in it. Returns why it is not in the game, and none
         * for one that is.
         */
        const char* find(const model::PropertyStorage& property, LookedProperty& looked)
        {
            const auto* gameVM = F4SEVR::getGameVM();
            _vm = gameVM ? gameVM->m_virtualMachine : nullptr;
            if (!_vm) {
                return "Papyrus is not running";
            }
            if (!looked.form) {
                return "its form is not in the game";
            }
            const auto handle = _vm->GetHandlePolicy()->Create(static_cast<F4SEVR::UInt32>(looked.form->GetFormType()), looked.form);
            _vm->GetObjectIdentifier(handle, property.script.empty() ? "ScriptObject" : property.script.c_str(), 1, &_script, 0);
            if (!_script) {
                return "its form has no such script attached";
            }
            if (looked.type != _script->m_typeInfo) {
                F4SEVR::VMPropertyInfo info{};
                info.index = -1;
                F4SEVR::BSFixedString name(property.name.c_str());
                F4SEVR::GetVMPropertyInfo(_script->m_typeInfo, &info, &name, true);
                looked.type = _script->m_typeInfo;
                looked.index = info.index;
            }
            _index = looked.index;
            return _index < 0 ? "its script has no property of that name" : nullptr;
        }

        F4SEVR::VirtualMachine* _vm = nullptr;
        F4SEVR::VMIdentifier* _script = nullptr;
        F4SEVR::SInt32 _index = -1;
    };
}

namespace mods_config
{
    /**
     * The value a property of a script has in the game now, in the type the script keeps it in: a bool, a
     * whole number, a number or a text. None for one that is not in the game, and for one of another type.
     */
    std::optional<model::Value> getProperty(const model::PropertyStorage& property)
    {
        ScriptProperty found(property);
        if (!found) {
            return std::nullopt;
        }
        F4SEVR::VMValue value;
        found.get(value);
        switch (value.GetTypeEnum()) {
        case F4SEVR::VMValue::kType_Bool:
            return value.data.b;
        case F4SEVR::VMValue::kType_Int:
            return static_cast<int>(value.data.i);
        case F4SEVR::VMValue::kType_Float:
            return value.data.f;
        case F4SEVR::VMValue::kType_String: {
            const auto* text = value.data.GetStr()->c_str();
            return std::string(text ? text : "");
        }
        default:
            warnOnce(property, lookedProperty(property), "its value is no bool, number or text");
            return std::nullopt;
        }
    }

    /**
     * Sets a property of a script in the game's memory, as MCM's own menu does, in the type of the value.
     * The game keeps it in the save. Returns false for one that is not in the game.
     */
    bool setProperty(const model::PropertyStorage& property, const model::Value& value)
    {
        ScriptProperty found(property);
        if (!found) {
            return false;
        }
        F4SEVR::VMValue newValue;
        std::visit(
            [&newValue]<typename T>(const T& typed) {
                if constexpr (std::is_same_v<T, bool>) {
                    newValue.SetBool(typed);
                } else if constexpr (std::is_same_v<T, int>) {
                    newValue.SetInt(typed);
                } else if constexpr (std::is_same_v<T, float>) {
                    newValue.SetFloat(typed);
                } else {
                    newValue.SetString(typed.c_str());
                }
            },
            value);
        found.set(property.name.c_str(), newValue);
        return true;
    }

    /**
     * The texts of the options a choice reads in the game, as `mcm_vr.dll` gives them to MCM's menu. Of a
     * FormList, the name of each of its forms, or its editor ID for one with no name, and then those of the
     * forms a script added to it. Of a property of a script, the texts of its array.
     * None for a FormList that is not in the game, and for a property that is not, or is no array of texts.
     */
    std::optional<std::vector<std::string>> getOptionTexts(const model::GameOptions& options)
    {
        if (options.property.empty()) {
            const auto dataHandler = RE::TESDataHandler::GetSingleton();
            const auto* list = dataHandler ? dataHandler->LookupForm<RE::BGSListForm>(options.formId, options.plugin) : nullptr;
            if (!list) {
                logger::warn("The FormList {:X} of '{}' is not in the game: its choice has no options", options.formId, options.plugin);
                return std::nullopt;
            }
            std::vector<std::string> texts;
            list->ForEachForm([&texts](RE::TESForm* form) {
                const auto name = RE::TESFullName::GetFullName(*form);
                const auto* editorId = form->GetFormEditorID();
                texts.emplace_back(!name.empty() ? name : std::string_view(editorId ? editorId : ""));
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return texts;
        }

        const model::PropertyStorage property{ .plugin = options.plugin,
            .formId = options.formId,
            .script = options.script,
            .name = options.property,
            .type = model::StoredType::String };
        ScriptProperty found(property);
        if (!found) {
            return std::nullopt;
        }
        F4SEVR::VMValue value;
        found.get(value);
        if (value.GetTypeEnum() != F4SEVR::VMValue::kType_StringArray) {
            warnOnce(property, lookedProperty(property), "its value is no array of texts");
            return std::nullopt;
        }
        std::vector<std::string> texts;
        if (const auto* array = value.data.arr) {
            for (F4SEVR::UInt32 i = 0; i < array->arr.count; ++i) {
                const auto* text = array->arr.entries[i].data.GetStr()->c_str();
                texts.emplace_back(text ? text : "");
            }
        }
        return texts;
    }

    /**
     * The name or the description of a form, as `mcm_vr.dll` gives them to MCM's menu: the text of the form's name
     * or description component. Empty for a form that is not in the game, or has no such component.
     */
    std::string getFormText(const model::GameText& text)
    {
        const auto dataHandler = RE::TESDataHandler::GetSingleton();
        auto* const form = dataHandler ? dataHandler->LookupForm(text.formId, text.plugin) : nullptr;
        if (!form) {
            logger::warn("The form {:X} of '{}' is not in the game: its row shows its own text", text.formId, text.plugin);
            return {};
        }
        if (text.kind == model::GameText::Kind::FormName) {
            return std::string(RE::TESFullName::GetFullName(*form));
        }
        auto* const description = form->As<RE::TESDescription>();
        if (!description) {
            return {};
        }
        RE::BSStringT<char> shown;
        description->GetDescription(shown, form);
        return shown.c_str() ? std::string(shown.c_str()) : std::string();
    }
}
