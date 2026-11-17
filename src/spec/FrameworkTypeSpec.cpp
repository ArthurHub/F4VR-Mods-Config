#include "FrameworkTypeSpec.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <span>
#include <tuple>

#include "model/Colors.h"
#include "model/Haptics.h"
#include "model/SettingValues.h"
#include "model/Transforms.h"
#include "vrcf/InputBindingParser.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a transform's parts and of a color's preset, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 6> TRANSFORM_POSITION_FIELDS{ "min", "max", "step", "x", "y", "z" };
        constexpr std::array<std::string_view, 6> TRANSFORM_ROTATION_FIELDS{ "min", "max", "step", "heading", "roll", "attitude" };
        constexpr std::array<std::string_view, 3> TRANSFORM_RANGE_FIELDS{ "min", "max", "step" };
        constexpr std::array<std::string_view, 3> PRESET_FIELDS{ "rgb", "alpha", "text" };

        /**
         * The presets of a color, from its `valueOptions`: its own `presets`, or the list of the config spec's
         * `sharedPresetLists` its `sharedPresets` names, each a color's table with an optional `text`, as
         * `{ "rgb": [255, 180, 107], "text": "Warm white" }`. A preset that is no color, or has no `rgb`, is left out, and so
         * are those past the most the editor's line holds, each reported. A color with neither has no presets.
         */
        std::vector<model::ColorPreset> loadColorPresets(const Json& valueOptions, ConfigSpecReading& reading)
        {
            const auto own = valueOptions.find("presets");
            const Json* list = own != valueOptions.end() ? &*own : nullptr;
            std::string sharedName;
            if (valueOptions.contains("sharedPresets")) {
                const Into into(reading, "sharedPresets");
                if (list) {
                    reading.report(model::Severity::Warning, "It has its own presets and names a shared list of them too: its own presets are used.");
                } else {
                    sharedName = getText(valueOptions, "sharedPresets");
                    list = findSharedList(reading.sharedPresetLists, sharedName);
                    if (!list) {
                        reading.report(model::Severity::Warning,
                            std::format("It names \"{}\", which is no list of the config spec's sharedPresetLists: it has no presets.", sharedName));
                        return {};
                    }
                }
            }
            if (!list) {
                return {};
            }
            const bool isOwn = sharedName.empty();
            if (!list->is_array()) {
                const auto which = isOwn ? "Its presets are"s : std::format("The shared list \"{}\" is", sharedName);
                reading.report(model::Severity::Warning, std::format("{} not a list: it has no presets.", which));
                return {};
            }

            std::vector<model::ColorPreset> presets;
            for (std::size_t place = 0; place < list->size(); ++place) {
                const auto& preset = (*list)[place];
                std::optional<Into> into;
                if (isOwn) {
                    into.emplace(reading, "presets", place);
                }
                const auto which = isOwn ? std::format("Preset {}", place + 1) : std::format("Preset {} of the shared list \"{}\"", place + 1, sharedName);
                if (!preset.is_object()) {
                    reading.report(model::Severity::Error, std::format(R"({} is left out: it is not an object as {{ "rgb": [255, 180, 107], "text": "Warm white" }}.)", which));
                    continue;
                }
                if (isOwn) {
                    checkFields(preset, PRESET_FIELDS, reading);
                }
                // its color alone: a field that is not read is reported already
                Json table = Json::object();
                for (const auto* key : { "rgb", "alpha" }) {
                    if (const auto channel = preset.find(key); channel != preset.end()) {
                        table[key] = *channel;
                    }
                }
                const auto color = model::parseColorTable(model::tomlText(table), model::TableForm::Toml);
                if (!color || !color->rgb) {
                    reading.report(model::Severity::Error, std::format("{} is left out: {}.", which, color ? "it has no rgb"s : color.error()));
                    continue;
                }
                presets.push_back(
                    { .rgb = *color->rgb, .alpha = color->alpha, .text = isOwn ? readShownText(preset, "text", reading) : getShownText(preset, "text", reading.translations) });
            }
            if (presets.size() > model::MAX_COLOR_PRESETS) {
                reading.report(model::Severity::Warning,
                    std::format("It has {} presets, and its editor shows {} at the most: the rest are left out.", presets.size(), model::MAX_COLOR_PRESETS));
                presets.resize(model::MAX_COLOR_PRESETS);
            }
            return presets;
        }

        /**
         * A list of words of a binding's `valueOptions`, `hands`, `types` or `buttons`, read as the framework reads them
         * in a binding. A word that is none of its part's is left out, and reported, and so is a value that is not a list.
         * None for a list that is not there: the setting takes any.
         */
        template <typename T, typename Parse>
        std::vector<T> loadBindingWords(const Json& options, const std::string_view key, Parse parse, const std::string_view part, ConfigSpecReading& reading)
        {
            std::vector<T> words;
            const auto found = options.find(key);
            if (found == options.end()) {
                return words;
            }
            if (!found->is_array()) {
                const Into into(reading, key);
                reading.report(model::Severity::Warning, std::format("It is not a list of words: the binding takes any {}.", part.substr(part.find(' ') + 1)));
                return words;
            }
            for (std::size_t place = 0; place < found->size(); ++place) {
                const auto& item = (*found)[place];
                const auto word = item.is_string() ? parse(item.get<std::string>()) : std::nullopt;
                if (!word) {
                    const Into into(reading, key, place);
                    reading.report(model::Severity::Warning, std::format("{} is not {}, as the framework names it: it is left out.", item.dump(), part));
                    continue;
                }
                words.push_back(*word);
            }
            return words;
        }

        /**
         * The range of a transform's sliders in an object, over the range it is given: its `min`, `max` and `step`, each
         * optional. One whose `min` is not below its `max`, or whose `step` is not above 0, is an error, and gives the range
         * it was given.
         */
        model::TransformAxis loadTransformRange(const Json& range, const model::TransformAxis& over, ConfigSpecReading& reading)
        {
            auto axis = over;
            axis.min = getNumber(range, "min", over.min);
            axis.max = getNumber(range, "max", over.max);
            axis.step = getNumber(range, "step", over.step);
            if (axis.min < axis.max && axis.step > 0.0f) {
                return axis;
            }
            reading.report(model::Severity::Error, "Its range is none: its min has to be below its max, and its step above 0. It has the range of what it is in.");
            return over;
        }

        /**
         * A part of a transform: its key of `valueOptions`, the fields it has, its first axis, its own range, and the key of
         * each of its axes, which a scale has none of.
         */
        struct TransformPart
        {
            const char* key = nullptr;
            std::span<const std::string_view> fields;
            std::size_t first = 0;
            model::TransformAxis range;
            std::span<const std::string_view> axes;
        };

        const std::array TRANSFORM_PARTS{
            TransformPart{ .key = "position",
                .fields = TRANSFORM_POSITION_FIELDS,
                .first = model::TRANSFORM_POSITION,
                .range = { .min = -50.0f, .max = 50.0f, .step = 1.0f, .on = true },
                .axes = std::span(TRANSFORM_POSITION_FIELDS).subspan(3) },
            TransformPart{ .key = "rotation",
                .fields = TRANSFORM_ROTATION_FIELDS,
                .first = model::TRANSFORM_ROTATION,
                .range = { .min = -180.0f, .max = 180.0f, .step = 1.0f, .on = true },
                .axes = std::span(TRANSFORM_ROTATION_FIELDS).subspan(3) },
            TransformPart{ .key = "scale",
                .fields = TRANSFORM_RANGE_FIELDS,
                .first = model::TRANSFORM_SCALE,
                .range = { .min = 0.1f, .max = 3.0f, .step = 0.1f, .on = true },
                .axes = {} },
        };

        /**
         * The axes of a transform, of its `valueOptions`, which is none where it has none: each of its parts is `true`, or
         * left out, for on with its own range, `false` for off, or an object of its range, over which an axis of it can have
         * one of its own. Any other value is on with its own range, and is reported.
         */
        std::array<model::TransformAxis, model::TRANSFORM_AXES> loadTransformAxes(const Json* options, ConfigSpecReading& reading)
        {
            std::array<model::TransformAxis, model::TRANSFORM_AXES> axes;
            for (const auto& part : TRANSFORM_PARTS) {
                auto range = part.range;
                const Json* value = options && options->contains(part.key) ? &(*options)[part.key] : nullptr;
                std::optional<Into> into;
                if (value) {
                    into.emplace(reading, part.key);
                    if (value->is_boolean()) {
                        range.on = value->get<bool>();
                    } else if (value->is_object()) {
                        checkFields(*value, part.fields, reading);
                        range = loadTransformRange(*value, range, reading);
                    } else {
                        reading.report(model::Severity::Warning, std::format("Its {} is not true, false or a range: it is on, with its own range.", part.key));
                    }
                }
                for (std::size_t i = 0; i < (std::max)(part.axes.size(), std::size_t{ 1 }); ++i) {
                    axes[part.first + i] = range;
                    if (i < part.axes.size() && value && value->is_object() && value->contains(part.axes[i])) {
                        const Into intoAxis(reading, part.axes[i]);
                        const auto& axis = (*value)[part.axes[i]];
                        checkFields(axis, TRANSFORM_RANGE_FIELDS, reading);
                        axes[part.first + i] = axis.is_object() ? loadTransformRange(axis, range, reading) : range;
                    }
                }
            }
            return axes;
        }
    }

    /**
     * The `valueOptions` of a framework type (`cf.*`), over the ones every setting has: the hands, the ways and the
     * buttons a binding takes, and whether it suppresses its buttons, a haptic's options, which are the menu's, a
     * color's alpha and presets, and the ranges of a transform's parts that are on. `options` is none for a setting
     * with no `valueOptions`, which a haptic and a transform still have theirs for.
     */
    void loadFrameworkValueOptions(const model::SettingType type, const Json* options, model::ValueOptions& valueOptions, ConfigSpecReading& reading)
    {
        if (options && type == model::SettingType::InputBinding) {
            valueOptions.suppress = getBool(*options, "suppress");
            valueOptions.hands = loadBindingWords<vrcf::Hand>(*options, "hands", vrcf::parseHand, "a hand", reading);
            valueOptions.types = loadBindingWords<vrcf::ActivationType>(*options, "types", vrcf::parseActivationType, "a type", reading);
            valueOptions.buttons = loadBindingWords<vr::EVRButtonId>(*options, "buttons", vrcf::parseButton, "a button", reading);
        }
        if (type == model::SettingType::Haptic) {
            valueOptions.options = model::hapticChoiceOptions();
        }
        if (options && type == model::SettingType::Color) {
            const auto alpha = options->find("alpha");
            valueOptions.alpha = alpha == options->end() || !alpha->is_boolean() || alpha->get<bool>();
            valueOptions.presets = loadColorPresets(*options, reading);
        }
        if (type == model::SettingType::Transform) {
            valueOptions.transformAxes = loadTransformAxes(options, reading);
        }
    }

    /**
     * Checks the presets of the config spec's shared preset lists once, as several colors can read one: the fields of a
     * preset, and its text. A preset that is no color is reported as a color reads the list.
     */
    void checkSharedPresetLists(ConfigSpecReading& reading)
    {
        if (!reading.sharedPresetLists) {
            return;
        }
        const Into into(reading, "sharedPresetLists");
        for (const auto& [name, list] : reading.sharedPresetLists->items()) {
            if (!list.is_array()) {
                continue;
            }
            const Into intoList(reading, name);
            for (std::size_t place = 0; place < list.size(); ++place) {
                if (list[place].is_object()) {
                    const Into intoPreset(reading, "", place);
                    checkFields(list[place], PRESET_FIELDS, reading);
                    std::ignore = readShownText(list[place], "text", reading);
                }
            }
        }
    }
}
