#include "Bindings.h"

#include <format>
#include <optional>
#include <string_view>

#include <imgui.h>

#include "MenuControls.h"
#include "Theme.h"
#include "model/InputBindings.h"
#include "vrui/BindingPrompt.h"

namespace
{
    using namespace mods_config;

    // the folder of the mod's copy of the framework's binding icons, under the game's `Data\Textures`
    constexpr std::string_view ICONS_FOLDER = "ModsConfigFO4\\";

    // every icon is 80 px tall with its button in the middle, and is drawn at this part of its size, 40 px, inside the
    // 48 px line with room above and below, so a thumbstick does not touch the frame: every button at one scale
    constexpr float ICON_SCALE = 0.5f;
    constexpr float ICON_HEIGHT = 80.0f;

    // the width of an icon the game does not tell the size of, as the framework draws one
    constexpr float ICON_WIDTH = 60.0f;

    /**
     * The controller a hand of a binding is: the primary hand is the right one, unless the player is left-handed.
     */
    vrcf::Hand controllerOf(const vrcf::Hand hand, const bool leftHanded)
    {
        switch (hand) {
        case vrcf::Hand::Primary:
            return leftHanded ? vrcf::Hand::Left : vrcf::Hand::Right;
        case vrcf::Hand::Offhand:
            return leftHanded ? vrcf::Hand::Right : vrcf::Hand::Left;
        default:
            return hand;
        }
    }

    /**
     * A binding with its hands as the controllers they are, by the player's handedness: the framework then draws its
     * icons with no call to the game, which tells the handedness only while it runs. A modifier on the binding's own
     * hand names that controller.
     */
    vrcf::InputBinding onControllers(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        auto resolved = binding;
        resolved.hand = controllerOf(binding.hand, leftHanded);
        if (resolved.modifier) {
            resolved.modifier->hand = controllerOf(binding.modifier->hand.value_or(binding.hand), leftHanded);
        }
        return resolved;
    }

    /**
     * The binding a value of a binding setting is. None for a value that is no binding.
     */
    std::optional<vrcf::InputBinding> bindingOf(const model::Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        if (!text) {
            return std::nullopt;
        }
        auto binding = model::parseBinding(*text);
        return binding ? std::optional(std::move(*binding)) : std::nullopt;
    }

    /**
     * The width an icon is drawn at: its own, at a part of its size.
     */
    float iconWidth(const std::string& icon, const float scale, const ui::ImageDrawer& drawer)
    {
        const auto size = drawer.textureSize ? drawer.textureSize(icon) : std::nullopt;
        return (size && size->y > 0.0f ? size->x * ICON_HEIGHT / size->y : ICON_WIDTH) * scale;
    }
}

namespace mods_config::ui
{
    /**
     * Whether a hand of a binding is the right controller: the primary hand is, unless the player is left-handed.
     */
    bool onRightController(const vrcf::Hand hand, const bool leftHanded)
    {
        return controllerOf(hand, leftHanded) == vrcf::Hand::Right;
    }

    /**
     * The icons of a binding, by their paths under the game's `Data\Textures`, as the framework draws its prompt: its
     * button, with how it is worked, on the controller of its hand, and a chord's modifier after it. None for what has
     * no icon: the system button, and a binding that is off.
     */
    std::vector<std::string> bindingIcons(const vrcf::InputBinding& binding, const bool leftHanded)
    {
        auto icons = vrui::bindingIconPaths(onControllers(binding, leftHanded));
        for (auto& icon : icons) {
            icon.insert(0, ICONS_FOLDER);
        }
        return icons;
    }

    /**
     * Whether two bindings are the same press: of one button, or one push of an axis, on one controller, worked in
     * one way, while one button is held, or none. Their times and their thresholds are not compared. A binding that is
     * off is no press.
     */
    bool samePress(const vrcf::InputBinding& binding, const vrcf::InputBinding& other, const bool leftHanded)
    {
        if (!binding.isEnabled() || !other.isEnabled()) {
            return false;
        }
        const auto one = onControllers(binding, leftHanded);
        const auto two = onControllers(other, leftHanded);
        if (one.hand != two.hand || one.type != two.type || one.modifier != two.modifier) {
            return false;
        }
        return one.type == vrcf::ActivationType::AxisDirection ? one.axis == two.axis && one.direction == two.direction : one.button == two.button;
    }

    /**
     * A value of a binding as the help line names it: its sentence. Empty for a value that is no binding.
     */
    std::string bindingValueName(const model::Value& value, const bool leftHanded)
    {
        const auto binding = bindingOf(value);
        return binding ? bindingSentence(*binding, leftHanded) : std::string();
    }

    /**
     * What the help line says of a binding that is the same press as other bindings of its mod, by their names:
     * `Also the press of Secondary binding.` Empty for one that is none, and for a value that is no binding.
     */
    std::string samePressHelp(const model::Setting& setting, const std::span<const model::Setting* const> bindings, model::Store& store, const bool leftHanded)
    {
        const auto value = store.value(setting);
        const auto binding = value.available ? bindingOf(value.value) : std::nullopt;
        if (!binding) {
            return {};
        }
        std::vector<std::string_view> names;
        for (const auto* other : bindings) {
            if (other == &setting) {
                continue;
            }
            const auto otherValue = store.value(*other);
            const auto otherBinding = otherValue.available ? bindingOf(otherValue.value) : std::nullopt;
            if (otherBinding && samePress(*binding, *otherBinding, leftHanded)) {
                names.emplace_back(other->text);
            }
        }
        if (names.empty()) {
            return {};
        }
        std::string list;
        for (std::size_t i = 0; i < names.size(); ++i) {
            list += std::format("{}{}", i == 0 ? "" : i + 1 == names.size() ? " and " : ", ", names[i]);
        }
        return std::format("Also the press of {}.", list);
    }

    /**
     * The icons of a binding from a left edge, around a middle, at a part of their size, tinted with a color: a chord's
     * two with a plus between them. Nothing for a binding with no icon. Returns their right edge, which is the left
     * edge where nothing is drawn.
     */
    float drawBindingIcons(const vrcf::InputBinding& binding, const float left, const float middle, const float scale, const ImU32 color, const bool leftHanded,
        const ImageDrawer& drawer)
    {
        const auto icons = bindingIcons(binding, leftHanded);
        if (icons.empty() || !drawer.drawIcon) {
            return left;
        }
        float right = left;
        const float iconHeight = ICON_HEIGHT * scale;
        const float iconTop = middle - iconHeight * 0.5f;
        for (std::size_t i = 0; i < icons.size(); ++i) {
            if (i > 0) {
                drawText(right + theme::SPACE_1, right + theme::SPACE_8, middle - theme::CONTROL_HEIGHT * 0.5f, theme::CONTROL_HEIGHT, color, "+");
                right += theme::SPACE_1 * 2.0f + textWidth("+");
            }
            const float width = iconWidth(icons[i], scale, drawer);
            drawer.drawIcon(icons[i], ImVec2(right, iconTop), ImVec2(right + width, iconTop + iconHeight), ImGui::GetColorU32(color));
            right += width;
        }
        return right;
    }

    /**
     * A binding that ends at the right, in a frame as wide as a dropdown's: its icons and its sentence in the color of
     * its value's look, the icons tinted as the text is, and the down arrow, as a press opens its editor, with the outline
     * in the full color while it is open. A value that is no binding is its text, as the file has it, in the frame of an
     * invalid value. One that is off is `None`, and one with no value has an empty frame. Returns its left edge.
     */
    float drawBinding(const float right, const float top, const std::string* value, const ValueLook look, const bool open, const bool leftHanded, const ImageDrawer& drawer)
    {
        const auto box = controlBox(right - DROPDOWN_WIDTH, top, DROPDOWN_WIDTH);
        const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
        const auto color = valueColor(look);
        drawFrame(box, look);
        if (open) {
            drawOutline(box.Min, box.Max, theme::color::GREEN);
        }

        float textLeft = box.Min.x + theme::SPACE_4;
        std::string text;
        if (value) {
            const auto binding = bindingOf(*value);
            text = binding ? bindingSentence(*binding, leftHanded) : *value;
            if (binding) {
                const float iconsRight = drawBindingIcons(*binding, box.Min.x + theme::SPACE_3, box.GetCenter().y, ICON_SCALE, color, leftHanded, drawer);
                if (iconsRight > box.Min.x + theme::SPACE_3) {
                    textLeft = iconsRight + theme::SPACE_2;
                }
            }
        }
        drawText(textLeft, arrowLeft - theme::SPACE_2, top, theme::CONTROL_HEIGHT, color, text);
        drawControlArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), ControlArrow::Down);
        return box.Min.x;
    }
}
