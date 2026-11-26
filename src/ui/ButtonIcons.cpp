#include "ButtonIcons.h"

namespace mods_config::ui
{
    /**
     * The framework's icon of a config spec's button icon, which `controls::drawIcon` draws at `BUTTON_ICON_SIZE` as the
     * design system's ButtonRow does. None for none.
     */
    f4cf::imgui::controls::Icon buttonIcon(const model::ButtonIcon icon)
    {
        using f4cf::imgui::controls::Icon;
        switch (icon) {
        case model::ButtonIcon::None:
            return Icon::None;
        case model::ButtonIcon::Play:
            return Icon::Play;
        case model::ButtonIcon::Stop:
            return Icon::Stop;
        case model::ButtonIcon::Bolt:
            return Icon::Bolt;
        case model::ButtonIcon::Target:
            return Icon::Target;
        case model::ButtonIcon::Check:
            return Icon::Check;
        case model::ButtonIcon::Window:
            return Icon::Window;
        case model::ButtonIcon::External:
            return Icon::External;
        case model::ButtonIcon::Link:
            return Icon::Link;
        case model::ButtonIcon::Gear:
            return Icon::Gear;
        case model::ButtonIcon::Import:
            return Icon::Import;
        case model::ButtonIcon::Export:
            return Icon::Export;
        case model::ButtonIcon::Copy:
            return Icon::Copy;
        case model::ButtonIcon::Eye:
            return Icon::Eye;
        case model::ButtonIcon::Hand:
            return Icon::Hand;
        case model::ButtonIcon::Sound:
            return Icon::Sound;
        case model::ButtonIcon::Pin:
            return Icon::Pin;
        case model::ButtonIcon::Refresh:
            return Icon::Refresh;
        case model::ButtonIcon::Trash:
            return Icon::Trash;
        }
        return Icon::None;
    }
}
