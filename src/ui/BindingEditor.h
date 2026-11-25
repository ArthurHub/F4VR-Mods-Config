#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "HelpLine.h"
#include "PageImages.h"
#include "Sliders.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "vrcf/VRControllersManager.h"

namespace mods_config::ui
{
    /**
     * What the editor of a binding tells of the frame that is drawn.
     */
    struct BindingEdited
    {
        // the part, or the option of an open list, the ray is on, for the help line. None while the ray is on neither
        std::optional<PointedRow> pointed;

        // a press opened the editor, changed a part, opened a list, or kept or gave back the binding
        bool pulse = false;
    };

    /**
     * The editor of the binding settings of the page that is shown, as the design system's InputBinding has it: a
     * popup in the page, under the binding's row or over it, which builds the binding from its parts. Each change sets
     * the binding's pending value in the store as it is made, so a `live` binding is previewed. Done, or a press beside
     * the editor, keeps it, and Cancel and Back give back the value it had when it opened.
     * It points into the page that is shown, so it is reset as another page is.
     */
    class BindingEditor
    {
    public:
        BindingEdited edit(const model::Setting& setting, model::Store& store, const ImRect& row, bool pressed, bool leftHanded, const ImageDrawer& drawer);
        bool isOpen(const model::Setting& setting) const;

        const model::Setting* openSetting() const
        {
            return _open;
        }

        void endFrame();
        bool cancel();
        void reset();

        /**
         * A part of a binding, which a list of the editor is open for.
         */
        enum class Part : std::uint8_t
        {
            None,
            Hand,
            How,
            Button,
            Holding,
        };

    private:
        struct Frame;

        void open(const model::Setting& setting, model::Store& store);
        std::optional<float> stepTime(float least, float most, float current, const ImVec2& end, bool pointed);
        bool backClosed(const model::Setting& setting, model::Store& store);
        Frame frameOf(const model::Setting& setting, model::Store& store, bool leftHanded, const ImageDrawer& drawer, BindingEdited& edited, bool sided) const;
        void editParts(Frame& frame);
        void apply(const model::Setting& setting, model::Store& store) const;
        std::optional<std::size_t> dropdown(Frame& frame, Part part, std::string_view name, const ImVec2& at, std::string_view text, bool enabled,
            const std::vector<std::string>& texts, const std::vector<std::string>& helps, std::optional<std::size_t> chosen, std::string help);
        void editHand(Frame& frame);
        void editButton(Frame& frame, vr::EVRButtonId bound);
        void editHow(Frame& frame, vr::EVRButtonId bound);
        void editDirection(Frame& frame);
        void editTime(Frame& frame);
        void editHolding(Frame& frame);
        void editSuppress(Frame& frame);
        void drawHeader(const Frame& frame) const;
        void drawFooter(Frame& frame);

        // the binding setting whose editor is open, and none
        const model::Setting* _open = nullptr;

        // its editor was open in the frame that is drawn
        bool _openSeen = false;

        // the value it had as its editor opened, which Cancel and Back give back
        std::string _before;

        // the parts of the binding as they are edited: those its type does not use keep their values, so a type that is
        // picked again has them
        vrcf::InputBinding _binding;

        // the way the binding is worked while it is on, which a hand picked after None gives it again
        vrcf::ActivationType _typeOn = vrcf::ActivationType::Tap;

        // the part whose list is open
        Part _list = Part::None;

        // Back asked to close the list that is open, or to give back the binding, as the next frame draws the editor
        bool _closeList = false;
        bool _cancel = false;

        // the time or the threshold while its slider is dragged, which is set as the trigger is let go
        std::optional<float> _dragged;

        // how far the thumbstick is pushed to a side in the frame that is drawn, and the steps it gives the slider, which
        // are the page sliders' own, as a setting of the slider's range
        model::Setting _stepped;
    };
}
