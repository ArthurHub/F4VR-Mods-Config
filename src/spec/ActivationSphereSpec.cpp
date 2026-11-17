#include "ActivationSphereSpec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <variant>

#include "StorageSpec.h"
#include "model/ActivationSpheres.h"
#include "model/InputBindings.h"

namespace mods_config::spec
{
    namespace
    {
        // the fields of a `cf.activationSphere` row, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 9> SPHERE_FIELDS{ "type", "id", "text", "help", "storage", "keys", "applies", "visibleWhen", "enabledWhen" };
        // a key of a sphere's `keys`, written as an object:
        constexpr std::array<std::string_view, 8> SPHERE_KEY_FIELDS{ "text", "help", "default", "valueOptions", "applies", "visibleWhen", "enabledWhen", "warnings" };
        constexpr std::array<std::string_view, 5> SPHERE_ICONS_FIELDS{ "text", "help", "valueOptions", "visibleWhen", "enabledWhen" };

        // what a made row's path of a file has in place of the config spec's `modName`, the folder of its mod's assets
        constexpr std::string_view MADE_MOD_NAME = "<modName>";

        /**
         * Where a key of an activation sphere has its row: first what a player changes most, and then the sections that fold.
         */
        enum class SpherePart : std::uint8_t
        {
            Main,
            Zone,
            Activation,
            Visuals,
        };

        // the sections that fold of a sphere, by their parts after Main: the zone's heading names its size, which its scale
        // sets and a player looks for
        constexpr std::array<const char*, 3> SPHERE_SECTIONS{ "Position and size", "Activation", "Visuals" };

        /**
         * A key of an activation sphere's section, as the framework reads it (`loadWandActivationConfig`).
         */
        struct SphereKey
        {
            std::string_view name;
            SpherePart part;

            // the key this one depends on, and none: a haptic has no row while its binding is off, and the look of an icon
            // and of a sphere is disabled while it is never shown, and hidden with the key that says when it is
            std::string_view control;

            // it has no row unless the config spec's `keys` names it
            bool namedOnly = false;

            // the fields of its row, as a config spec writes a setting: its author's are over them. A default is the
            // framework's for the key, and a range is one a key is tuned in. A power-armor zone has no default: the framework
            // uses the zone for one a file leaves out, which no value of the key says
            const char* row = nullptr;
        };

        constexpr auto SPHERE_PRIMARY_BINDING = "kPrimaryBinding";
        constexpr auto SPHERE_ZONE_PA = "tZonePA";
        constexpr auto SPHERE_STYLE = "sSphereStyle";
        constexpr auto SPHERE_SHOW_ICON = "sShowIcon";
        constexpr auto SPHERE_SHOW_BINDING_ICON = "sShowBindingIcon";
        constexpr auto SPHERE_BINDING_ICON = "tBindingIcon";

        // the name in `keys` of a sphere's Icons row, which is no key of its section
        constexpr auto SPHERE_ICONS = "icons";

        constexpr std::array SPHERE_KEYS{
            SphereKey{ SPHERE_PRIMARY_BINDING,
                SpherePart::Main,
                "",
                false,
                R"({ "type": "cf.inputBinding", "text": "Activate", "help": "What fires it with a hand in the zone.",
                "default": { "hand": "primary", "type": "press", "button": "trigger" }, "valueOptions": { "suppress": true } })" },
            SphereKey{ "sEntryHaptic",
                SpherePart::Main,
                "",
                false,
                R"({ "type": "cf.haptic", "text": "Entry haptic", "help": "Played once as a hand enters the zone.", "default": "Tick" })" },
            SphereKey{ "sShowIcon",
                SpherePart::Main,
                "",
                false,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "visibility", "text": "Show icon", "help": "When the icon at the middle of the zone is drawn.", "default": "wheninside",
                "valueOptions": { "options": [ { "value": "never", "text": "Never", "help": "Not drawn." },
                    { "value": "always", "text": "Always", "help": "Drawn all the time, which shows where the zone is." },
                    { "value": "wheninside", "text": "When inside", "help": "Drawn while a hand that works the gesture is in the zone." },
                    { "value": "whenavailable", "text": "When available", "help": "Drawn while the gesture can be used, as one that needs a drawn weapon while one is drawn." } ] } })" },
            SphereKey{ "tZone",
                SpherePart::Zone,
                "",
                false,
                R"({ "type": "cf.transform", "text": "Zone", "help": "Where the zone is on its node and how big it is: its scale is the zone's diameter. Rotation does nothing to a sphere.",
                "default": { "pos": [0.0, 0.0, 0.0], "scale": 1.0 } })" },
            SphereKey{ SPHERE_ZONE_PA,
                SpherePart::Zone,
                "",
                false,
                R"({ "type": "cf.transform", "text": "Zone in power armor", "help": "Where the zone is in power armor and how big it is: its scale is the zone's diameter." })" },
            SphereKey{ "sPrimaryHaptic",
                SpherePart::Activation,
                SPHERE_PRIMARY_BINDING,
                false,
                R"({ "type": "cf.haptic", "text": "Primary haptic", "help": "Played as its binding fires.", "default": "DoubleClick" })" },
            SphereKey{ "kSecondaryBinding",
                SpherePart::Activation,
                "",
                false,
                R"({ "type": "cf.inputBinding", "text": "Secondary activate", "help": "A second way to fire it, and None for no second way.",
                "default": { "type": "none" }, "valueOptions": { "suppress": true } })" },
            SphereKey{ "sSecondaryHaptic",
                SpherePart::Activation,
                "kSecondaryBinding",
                false,
                R"({ "type": "cf.haptic", "text": "Secondary haptic", "help": "Played as the second binding fires.", "default": "DoubleClick" })" },
            SphereKey{ "sIcon",
                SpherePart::Visuals,
                "sShowIcon",
                false,
                R"({ "type": "dropdown", "valueType": "string", "text": "Icon", "help": "The image drawn at the middle of the zone.",
                "default": "Textures\\<modName>\\f4cf\\activation-icon-hand.dds",
                "valueOptions": { "listFromFolder": { "path": "Textures\\<modName>\\f4cf", "mask": "activation-icon-*.dds" } } })" },
            SphereKey{ "cIconColor",
                SpherePart::Visuals,
                "sShowIcon",
                false,
                R"({ "type": "cf.color", "text": "Icon color", "help": "The tint of the icon, and its opacity.", "default": { "rgb": [255, 255, 255], "alpha": 230 } })" },
            SphereKey{ "fIconSize",
                SpherePart::Visuals,
                "sShowIcon",
                false,
                R"({ "type": "slider", "valueType": "float", "text": "Icon size", "help": "The icon's longer side, in game units.", "default": 1.0,
                "valueOptions": { "min": 0.1, "max": 10, "step": 0.1 } })" },
            SphereKey{ "sShowBindingIcon",
                SpherePart::Visuals,
                "",
                false,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "visibility", "text": "Show binding icon", "help": "When the icons of the buttons that fire it are drawn, in a row that faces you.", "default": "never",
                "valueOptions": { "options": [ { "value": "never", "text": "Never", "help": "Not drawn." },
                    { "value": "always", "text": "Always", "help": "Drawn all the time, which shows where the zone is." },
                    { "value": "wheninside", "text": "When inside", "help": "Drawn while a hand that works the gesture is in the zone." },
                    { "value": "whenavailable", "text": "When available", "help": "Drawn while the gesture can be used, as one that needs a drawn weapon while one is drawn." } ] } })" },
            SphereKey{ "sBindingIcons",
                SpherePart::Visuals,
                "sShowBindingIcon",
                false,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "bindingIcons", "text": "Binding icons", "help": "Which bindings have an icon. One the gesture can't use right now has none.", "default": "inside",
                "valueOptions": { "options": [ { "value": "inside", "text": "Of the hand inside", "help": "The bindings of a hand that is in the zone." },
                    { "value": "all", "text": "All", "help": "Both bindings." },
                    { "value": "primary", "text": "Primary", "help": "The primary binding only." },
                    { "value": "secondary", "text": "Secondary", "help": "The secondary binding only." } ] } })" },
            SphereKey{ "tBindingIcon",
                SpherePart::Visuals,
                "sShowBindingIcon",
                false,
                R"({ "type": "cf.transform", "text": "Binding icon position and size", "help": "Where the row of icons is from the zone's center: x right, y up, z toward your eyes. A positive x starts the row there, a negative x ends it there. Its scale is each icon's longer side, in game units.",
                "default": { "pos": [0.0, 0.0, 0.0], "scale": 1.0 },
                "valueOptions": { "position": { "min": -10, "max": 10, "step": 0.1 }, "rotation": false, "scale": { "min": 0.1, "max": 10, "step": 0.1 } } })" },
            SphereKey{ "cBindingIconColor",
                SpherePart::Visuals,
                "sShowBindingIcon",
                false,
                R"({ "type": "cf.color", "text": "Binding icon color", "help": "The tint of the binding icons, and their opacity.", "default": { "rgb": [255, 255, 255], "alpha": 230 } })" },
            SphereKey{ "sShowSphere",
                SpherePart::Visuals,
                "",
                false,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "visibility", "text": "Show sphere", "help": "When the sphere that marks the zone is drawn.", "default": "never",
                "valueOptions": { "options": [ { "value": "never", "text": "Never", "help": "Not drawn." },
                    { "value": "always", "text": "Always", "help": "Drawn all the time, which shows where the zone is." },
                    { "value": "wheninside", "text": "When inside", "help": "Drawn while a hand that works the gesture is in the zone." },
                    { "value": "whenavailable", "text": "When available", "help": "Drawn while the gesture can be used, as one that needs a drawn weapon while one is drawn." } ] } })" },
            SphereKey{ SPHERE_STYLE,
                SpherePart::Visuals,
                "sShowSphere",
                false,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "style", "text": "Sphere style", "help": "How the sphere looks.", "default": "white-subtle" })" },
            SphereKey{ "fSphereScale",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "slider", "valueType": "float", "text": "Sphere size", "help": "The drawn size against the zone. The zone itself stays as it is.", "default": 1.0,
                "valueOptions": { "min": 0.05, "max": 3, "step": 0.05 } })" },
            SphereKey{ "sSphereOrientation",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "dropdown", "valueType": "string", "madeWords": "orientation", "text": "Sphere orientation", "help": "Which way the sphere faces, which only a patterned look shows.", "default": "body",
                "valueOptions": { "options": [ { "value": "body", "text": "Body" }, { "value": "hmd", "text": "Head" }, { "value": "world", "text": "World" } ] } })" },
            SphereKey{ "cSphereColor",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "cf.color", "text": "Sphere color", "help": "The color of the sphere over its style's, and its opacity.", "default": { "rgb": [255, 255, 255], "alpha": 27 } })" },
            SphereKey{ "fSphereGlow",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "slider", "valueType": "float", "text": "Sphere glow", "help": "How bright the sphere is.", "default": 0.4, "valueOptions": { "min": 0, "max": 1, "step": 0.05 } })" },
            SphereKey{ "sSphereNif",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "dropdown", "valueType": "string", "text": "Sphere mesh", "help": "The mesh the sphere is drawn with.", "default": "",
                "valueOptions": { "listFromFolder": { "path": "Meshes\\<modName>\\f4cf", "mask": "*.nif", "noneText": "Style's mesh" } } })" },
            SphereKey{ "sSphereTexture",
                SpherePart::Visuals,
                "sShowSphere",
                true,
                R"({ "type": "dropdown", "valueType": "string", "text": "Sphere texture", "help": "The texture on the sphere's mesh.", "default": "",
                "valueOptions": { "listFromFolder": { "path": "Textures\\<modName>\\f4cf", "mask": "*-sphere.dds", "noneText": "Style's texture" } } })" },
            SphereKey{ "aSphereFalloff", SpherePart::Visuals, "sShowSphere", true, R"({ "type": "cf.falloff", "text": "Sphere falloff" })" },
        };

        using SphereWord = std::pair<std::string_view, std::string_view>;

        // the framework's presets of a sphere's look, a color and a strength each (`findSphereStylePreset`), as their names
        // are written and as the list says them
        constexpr std::array SPHERE_COLORS{ SphereWord{ "white", "White" },
            SphereWord{ "gray", "Gray" },
            SphereWord{ "cyan", "Cyan" },
            SphereWord{ "green", "Green" },
            SphereWord{ "purple", "Purple" },
            SphereWord{ "red", "Red" },
            SphereWord{ "amber", "Amber" },
            SphereWord{ "gold", "Gold" } };
        constexpr std::array<std::string_view, 4> SPHERE_STRENGTHS{ "full", "medium", "subtle", "low" };

        /**
         * The options of a sphere's look: each color with each strength, and `debug`.
         */
        Json sphereStyleOptions()
        {
            auto options = Json::array();
            for (const auto& [color, colorText] : SPHERE_COLORS) {
                for (const auto strength : SPHERE_STRENGTHS) {
                    options.push_back(Json{ { "value", std::format("{}-{}", color, strength) }, { "text", std::format("{}, {}", colorText, strength) } });
                }
            }
            options.push_back(Json{ { "value", "debug" }, { "text", "Debug" } });
            return options;
        }

        /**
         * Adds a condition to the `visibleWhen` or the `enabledWhen` a row has, which it then needs too.
         */
        void addCondition(Json& row, const char* key, Json condition)
        {
            row[key] = row.contains(key) ? Json{ { "all", Json::array({ std::move(condition), row[key] }) } } : std::move(condition);
        }

        /**
         * The id a key of a sphere's section gets from where it is kept, which a condition names it by.
         */
        std::string sphereKeyId(const model::FileStorage& section, const std::string_view key)
        {
            auto path = section.path;
            path.emplace_back(key);
            return idOf(model::FileStorage{ .file = section.file, .path = std::move(path) });
        }

        /**
         * Whether the `keys` of a sphere hide a key.
         */
        bool isHiddenKey(const Json& keys, const std::string_view key)
        {
            const auto found = keys.find(std::string(key));
            return found != keys.end() && found->is_boolean() && !found->get<bool>();
        }

        /**
         * A text of a made row with the config spec's `modName` in place of `<modName>`. Any other value is left as it is.
         */
        void nameModFolder(Json& value, const std::string_view modName)
        {
            if (!value.is_string()) {
                return;
            }
            auto& text = value.get_ref<std::string&>();
            if (const auto at = text.find(MADE_MOD_NAME); at != std::string::npos) {
                text.replace(at, MADE_MOD_NAME.size(), modName);
            }
        }

        /**
         * Lays the object a sphere's `keys` gives a made row by its name over the row, where it gives one: each of its
         * fields the row can have, of which `valueOptions` adds to the row's. A field it cannot have is reported and left out.
         */
        void applyOwnFields(Json& row, const Json& keys, const std::string_view name, const std::span<const std::string_view> fields, ConfigSpecReading& reading)
        {
            const auto own = keys.find(std::string(name));
            if (own == keys.end() || !own->is_object()) {
                return;
            }
            const Into into(reading, row[MADE_ROW_PATH].get_ref<const std::string&>());
            checkFields(*own, fields, reading);
            for (const auto& [field, value] : own->items()) {
                if (field == "valueOptions") {
                    row[field].merge_patch(value);
                } else if (std::ranges::find(fields, field) != fields.end()) {
                    row[field] = value;
                }
            }
        }

        /**
         * The row of a key of a sphere: the framework's, with what its object in `keys` gives over it, of which `valueOptions`
         * adds to the key's. A key with another that it depends on has that condition too. The framework's files of a key
         * that is a choice of them are in the folders of the config spec's mod, as the framework ships them.
         */
        Json sphereKeyRow(const SphereKey& key, const Json& keys, const model::FileStorage& section, ConfigSpecReading& reading)
        {
            const std::string name(key.name);
            auto row = Json::parse(key.row);
            if (row.contains("valueOptions") && row["valueOptions"].contains("listFromFolder")) {
                nameModFolder(row["default"], reading.modName);
                nameModFolder(row["valueOptions"]["listFromFolder"]["path"], reading.modName);
            }
            row[MADE_ROW_PATH] = "keys." + name;
            auto path = section.path;
            path.push_back(name);
            row["storage"] = Json{ { "file", section.file }, { "path", std::move(path) } };
            if (key.name == SPHERE_STYLE) {
                row["valueOptions"]["options"] = sphereStyleOptions();
            }

            applyOwnFields(row, keys, name, SPHERE_KEY_FIELDS, reading);
            if (!key.control.empty() && !isHiddenKey(keys, key.control)) {
                const auto id = sphereKeyId(section, key.control);
                if (key.control.starts_with('k')) {
                    addCondition(row,
                        "visibleWhen",
                        Json{ { "setting", id }, { "notEquals", model::bindingText(vrcf::VRControllersManager::DisabledBinding, model::TableForm::Toml) } });
                } else {
                    addCondition(row, "enabledWhen", Json{ { "setting", id }, { "notEquals", "never" } });
                }
            }
            return row;
        }

        /**
         * The default of a key of a sphere: the `default` of its object in `keys`, or the framework's.
         */
        Json sphereKeyDefault(const Json& keys, const std::string_view name)
        {
            if (const auto own = keys.find(std::string(name)); own != keys.end() && own->is_object() && own->contains("default")) {
                return own->at("default");
            }
            const auto key = std::ranges::find(SPHERE_KEYS, name, &SphereKey::name);
            return key != SPHERE_KEYS.end() ? Json::parse(key->row).value("default", Json()) : Json();
        }

        /**
         * When a sphere's Show word draws its icon, as the help of an Icons option says it.
         */
        std::string_view sphereShownWhen(const std::string_view word)
        {
            if (word == "always") {
                return "all the time";
            }
            return word == "whenavailable" ? "while it can be used" : "while a hand is in the zone";
        }

        /**
         * The Icons row of a sphere: a stepper of the icons it draws, whose options set the keys that say when the icon and
         * the binding icon are drawn, and also set the binding icon's place and size where it has a row. Each key turns on with the
         * word of its default, and the option of both defaults sets the binding icon's default, so the mod's defaults are an
         * option; the others place it by the default icon size: in the icon's place for the binding alone, and at the icon's
         * lower right for both. Its object in `keys` gives it its `text`, `help` and conditions, and `valueOptions`, whose
         * `options` replace the four.
         */
        Json sphereIconsRow(const Json& keys, const model::FileStorage& section, ConfigSpecReading& reading)
        {
            const auto showIcon = sphereKeyId(section, SPHERE_SHOW_ICON);
            const auto showBindingIcon = sphereKeyId(section, SPHERE_SHOW_BINDING_ICON);
            const bool hasPlace = !isHiddenKey(keys, SPHERE_BINDING_ICON);
            const auto defaultWord = [&keys](const std::string_view name) {
                const auto word = sphereKeyDefault(keys, name);
                return word.is_string() ? model::canonicalSphereWord(model::SphereWords::Visibility, {}, word.get<std::string>()) : std::string();
            };
            const auto defaultIcon = defaultWord(SPHERE_SHOW_ICON);
            const auto defaultBinding = defaultWord(SPHERE_SHOW_BINDING_ICON);
            // a key turns on with the word of its default, and with when inside for a default that is off
            const auto onWord = [](const std::string& word) {
                return word == "always" || word == "whenavailable" ? word : std::string("wheninside");
            };
            const auto iconOn = onWord(defaultIcon);
            const auto bindingOn = onWord(defaultBinding);
            const auto defaultPlace = sphereKeyDefault(keys, SPHERE_BINDING_ICON);
            const auto defaultSize = sphereKeyDefault(keys, "fIconSize");
            const double iconSize = defaultSize.is_number() ? defaultSize.get<double>() : 1.0;

            // the Show keys are its sets, and the binding icon's place its alsoSets, so a player who tunes the place keeps the
            // option. pos and scale are for an icon of size 1: x right and y down by pos, each icon scale big
            const auto option = [&](const std::string_view value,
                                    const std::string_view text,
                                    const std::string_view help,
                                    const std::string_view icon,
                                    const std::string_view binding,
                                    const std::optional<double> pos = std::nullopt,
                                    const std::optional<double> scale = std::nullopt) {
                Json written{ { "value", value }, { "text", text }, { "help", help }, { "sets", Json{ { showIcon, icon }, { showBindingIcon, binding } } } };
                if (!pos || !hasPlace) {
                    return written;
                }
                Json place;
                if (icon == defaultIcon && binding == defaultBinding && defaultPlace.is_object()) {
                    place = Json{ { "pos", defaultPlace.value("pos", Json::array({ 0.0, 0.0, 0.0 })) }, { "scale", defaultPlace.value("scale", 1.0) } };
                } else {
                    // 0 - x keeps the center's y a zero, where -x is -0
                    const double x = *pos * iconSize;
                    place = Json{ { "pos", Json::array({ x, 0.0 - x, 0.0 }) }, { "scale", scale.value_or(1.0) * iconSize } };
                }
                written["alsoSets"] = Json{ { sphereKeyId(section, SPHERE_BINDING_ICON), std::move(place) } };
                return written;
            };

            Json row{ { "type", "stepper" },
                { "valueType", "string" },
                { "id", sphereKeyId(section, SPHERE_ICONS) },
                { "text", "Icons" },
                { "help", "Which icons show at the zone: its icon, the icons of its bindings, or both. Visuals sets when each shows and how it looks." },
                { MADE_ROW_PATH, std::format("keys.{}", SPHERE_ICONS) } };
            const auto iconWhen = sphereShownWhen(iconOn);
            const auto bindingWhen = sphereShownWhen(bindingOn);
            const auto bothHelp = iconOn == bindingOn ? std::format("Its icon shows {}, and the icons of its bindings beside it.", iconWhen)
                                                      : std::format("Its icon shows {}, and the icons of its bindings beside it {}.", iconWhen, bindingWhen);
            row["valueOptions"]["options"] = Json::array({ option("none", "None", "No icon shows.", "never", "never"),
                option("icon", "Icon", std::format("Its icon shows {}.", iconWhen), iconOn, "never"),
                option("binding", "Binding", std::format("The icons of its bindings show {}, in the icon's place.", bindingWhen), "never", bindingOn, 0.0, 1.25),
                option("both", "Icon and binding", bothHelp, iconOn, bindingOn, 0.65, 1.0) });

            applyOwnFields(row, keys, SPHERE_ICONS, SPHERE_ICONS_FIELDS, reading);
            return row;
        }

        /**
         * Whether a sphere's power-armor zone has a row: its file has the key, or its object in `keys` gives the `default` of
         * a mod that has one of its own. With neither the framework uses the zone, so it is as if `keys` hid it, as it is with
         * a file that is not there: `keys` naming it then, of a file that is read, is reported.
         */
        bool hasZonePA(const Json& keys, const model::FileStorage& section, ConfigSpecReading& reading)
        {
            auto path = section.path;
            path.emplace_back(SPHERE_ZONE_PA);
            const auto values = reading.fileValues.find(section.file);
            const bool read = values != reading.fileValues.end();
            const auto own = keys.find(SPHERE_ZONE_PA);
            if ((read && values->second->contains(path)) || (own != keys.end() && own->is_object() && own->contains("default"))) {
                return true;
            }
            if (read && own != keys.end()) {
                const Into into(reading, std::format("keys.{}", SPHERE_ZONE_PA));
                reading.report(model::Severity::Warning, std::format("Its file \"{}\" has no {} and it gives it no default: it has no row.", section.file, SPHERE_ZONE_PA));
            }
            return false;
        }

        /**
         * The `keys` of a sphere, an object each of whose names is a key of its section and each of whose values is true,
         * false or an object. A name that is none is left out, which `sphereKeyRow` does by asking for the keys there are,
         * and is reported, and so is a value that is none, as it shows its key, and the primary binding hidden, which is shown.
         */
        Json sphereKeys(const Json& row, ConfigSpecReading& reading)
        {
            const auto found = row.find("keys");
            if (found == row.end()) {
                return Json::object();
            }
            const Into into(reading, "keys");
            if (!found->is_object()) {
                reading.report(model::Severity::Warning, "Its keys are not an object: each key is shown with its default name.");
                return Json::object();
            }
            std::vector<std::string_view> names{ SPHERE_ICONS };
            for (const auto& key : SPHERE_KEYS) {
                names.push_back(key.name);
            }
            checkFields(*found, names, reading);
            auto keys = *found;
            for (auto& [name, value] : keys.items()) {
                if (value.is_object() || (value.is_boolean() && (value.get<bool>() || name != SPHERE_PRIMARY_BINDING))) {
                    continue;
                }
                const Into intoKey(reading, name);
                if (value.is_boolean()) {
                    reading.report(model::Severity::Error, "The primary binding cannot be hidden: it is shown.");
                } else {
                    reading.report(model::Severity::Warning, std::format("Its value {} is not true, false or an object: it is shown.", value.dump()));
                }
                value = true;
            }
            return keys;
        }
    }

    /**
     * The section a `cf.activationSphere` row is, which the rows of a config spec are then read as: its heading, its main
     * rows, and Position and size, Activation and Visuals, which fold and start folded, each left out with no row. Each key that is
     * shown is an ordinary setting of the section's file. The main rows end with Icons (`sphereIconsRow`), which picks
     * the icons it draws, and Show icon is then under Visuals: unless `keys` hides Icons, or a Show key it sets, as
     * `icons: false` does. Its `applies` is its own, or the nearest above it, or `live`, as
     * the framework previews a key. Its storage is the section, in a TOML file: one that is not has no rows, and is
     * reported, and so is a file that has no such section, whose keys show their defaults.
     */
    Json sphereSection(const Json& row, const Inherited& above, ConfigSpecReading& reading)
    {
        checkFields(row, SPHERE_FIELDS, reading);
        const auto own = inherit(row, above, reading);
        auto section = Json::object();
        section["type"] = "section";
        section["content"] = Json::array();
        for (const auto* field : { "id", "text", "help", "visibleWhen", "enabledWhen" }) {
            if (row.contains(field)) {
                section[field] = row[field];
            }
        }
        if (findApplies(row)) {
            section["applies"] = row["applies"];
        } else if (own.applies == model::Applies::Unknown) {
            section["applies"] = "live";
        }

        const auto storage = loadStorage(row, own.defaultFile, model::StoredType::String, reading);
        if (std::holds_alternative<model::UnknownStorage>(storage)) {
            return section;
        }
        const auto* file = std::get_if<model::FileStorage>(&storage);
        if (!file || reading.files.at(file->file).format != model::FileFormat::Toml) {
            const Into into(reading, "storage");
            reading.report(model::Severity::Error, "An activation sphere is a section of a TOML file, and it is not: it has no rows.");
            return section;
        }
        // with no id of its own its client knows it by its section, the names of its path joined with dots
        if (getText(section, "id").empty()) {
            std::string id;
            for (const auto& name : file->path) {
                id += (id.empty() ? "" : ".") + name;
            }
            section["id"] = id;
        }
        if (const auto values = reading.fileValues.find(file->file); values != reading.fileValues.end() && !values->second->contains(file->path)) {
            const Into into(reading, "storage");
            reading.report(model::Severity::Warning,
                std::format("Its file \"{}\" has no section {}: each key shows its default, and a Save adds it.", file->file, values->second->pathName(file->path)));
        }

        const auto keys = sphereKeys(row, reading);
        const bool icons = !isHiddenKey(keys, SPHERE_ICONS) && !isHiddenKey(keys, SPHERE_SHOW_ICON) && !isHiddenKey(keys, SPHERE_SHOW_BINDING_ICON);
        std::array<Json, SPHERE_SECTIONS.size() + 1> rows{ Json::array(), Json::array(), Json::array(), Json::array() };
        for (const auto& key : SPHERE_KEYS) {
            const bool hiddenWithControl = key.control.starts_with('s') && isHiddenKey(keys, key.control);
            const bool shown = !isHiddenKey(keys, key.name) && !hiddenWithControl && (!key.namedOnly || keys.contains(std::string(key.name)));
            if (shown && (key.name != SPHERE_ZONE_PA || hasZonePA(keys, *file, reading))) {
                // with the Icons row, Show icon is the first of the icon's rows
                const auto part = icons && key.name == SPHERE_SHOW_ICON ? SpherePart::Visuals : key.part;
                rows[static_cast<std::size_t>(part)].push_back(sphereKeyRow(key, keys, *file, reading));
            }
        }
        if (icons) {
            rows[0].push_back(sphereIconsRow(keys, *file, reading));
        }
        section["content"] = std::move(rows[0]);
        for (std::size_t part = 0; part < SPHERE_SECTIONS.size(); ++part) {
            if (!rows[part + 1].empty()) {
                section["content"].push_back(Json{ { "type", "section" },
                    { "text", SPHERE_SECTIONS[part] },
                    { "isCollapsible", true },
                    { "collapsed", true },
                    { "content", std::move(rows[part + 1]) },
                    { MADE_ROW_PATH, "" } });
            }
        }
        return section;
    }
}
