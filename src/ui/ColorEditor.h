#pragma once

#include <array>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "HelpLine.h"
#include "Sliders.h"
#include "model/Colors.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * What the editor of a color tells of the frame that is drawn.
     */
    struct ColorEdited
    {
        // the part the ray is on, for the help line. None while the ray is on none
        std::optional<PointedRow> pointed;

        // a press opened the editor, took hold of the picker or of a channel, or kept or gave back the color, or the
        // thumbstick stepped a channel
        bool pulse = false;
    };

    /**
     * The editor of the color settings of the page that is shown, as the design system's Color has it: a popup in the
     * page, under the color's row or over it, with a picker, a slider for each channel, and swatches of presets. Each change sets the color's
     * pending value in the store as it is made, so a `live` color is previewed as it is dragged. Done, or a press beside
     * the editor, keeps it, and Cancel and Back give back the value it had when it opened.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class ColorEditor
    {
    public:
        ColorEditor();
        void startFrame(float thumbstickSideValue);
        ColorEdited edit(const model::Setting& setting, model::Store& store, const ImRect& row, bool pressed);
        bool isOpen(const model::Setting& setting) const;
        void endFrame();
        bool cancel();
        void reset();

    private:
        void start(const model::Setting& setting, const std::string& text);
        void apply(const model::Setting& setting, model::Store& store, bool rgb, bool alpha);

        // the color setting whose editor is open, and none
        const model::Setting* _open = nullptr;

        // its editor was open in the frame that is drawn
        bool _openSeen = false;

        // the value it had as its editor opened, which Before, Cancel and Back give back
        std::string _before;

        // the color as it is edited: the keys of its table, which a change adds the key of its part to, and the color, whose
        // channels are each a whole byte
        model::ColorTable _table;
        render::Color _color;

        // Back asked to give back the color, as the next frame draws the editor
        bool _cancel = false;

        // the channel that is dragged, and none
        std::optional<std::size_t> _dragged;

        // how far the thumbstick is pushed to a side in the frame that is drawn, and the steps it gives a channel, which
        // are the page sliders' own, as a setting of 0 to 255 by 1 for each channel
        float _thumbstickSideValue = 0.0f;
        Sliders _sliders;
        std::array<model::Setting, 4> _stepped;
    };
}
