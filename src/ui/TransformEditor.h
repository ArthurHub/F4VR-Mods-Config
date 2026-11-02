#pragma once

#include <array>
#include <optional>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "HelpLine.h"
#include "Sliders.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "model/Transforms.h"

namespace mods_config::ui
{
    /**
     * What the editor of a transform tells of the frame that is drawn.
     */
    struct TransformEdited
    {
        // the part the ray is on, for the help line. None while the ray is on none
        std::optional<PointedRow> pointed;

        // a press opened the editor, took hold of a slider, or kept or gave back the transform, or a step changed a slider
        bool pulse = false;
    };

    /**
     * The editor of the transform settings of the page that is shown, as the design system's Transform has it: a popup in
     * the page, under the transform's row or over it, with a slider for each axis of each part that is on. Each change
     * sets the transform's pending value in the store as it is made, so a `live` transform is previewed as it is dragged.
     * Done, or a press beside the editor, keeps it, and Cancel and Back give back the value it had when it opened.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class TransformEditor
    {
    public:
        void startFrame(float thumbstickSideValue);
        TransformEdited edit(const model::Setting& setting, model::Store& store, const ImRect& row, bool pressed);
        bool isOpen(const model::Setting& setting) const;
        void endFrame();
        bool cancel();
        void reset();

    private:
        void start(const model::Setting& setting, const std::string& text);

        // the transform setting whose editor is open, and none
        const model::Setting* _open = nullptr;

        // its editor was open in the frame that is drawn
        bool _openSeen = false;

        // the value it had as its editor opened, which Cancel and Back give back
        std::string _before;

        // the transform as it is edited: the keys of its table, which a change adds the key of its part to
        model::TransformTable _table;

        // Back asked to give back the transform, as the next frame draws the editor
        bool _cancel = false;

        // how far the thumbstick is pushed to a side in the frame that is drawn, and the sliders of the axes, the page
        // sliders' own, each with a setting of the axis's range
        float _thumbstickSideValue = 0.0f;
        Sliders _sliders;
        std::array<model::Setting, model::TRANSFORM_AXES> _axes;
    };
}
