#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "model/TransformTuning.h"
#include "vrui/UIContainer.h"
#include "vrui/UITextPanel.h"

namespace mods_config
{
    /**
     * Adjust in world: a transform tuned with the controllers while the player looks at what it places, from a column
     * on the offhand wrist of the setting's name, its values, its controls, and Cancel and Done. All controller input is
     * taken from the game meanwhile. Each change is the setting's pending value in the store, which its mod's client
     * previews. It points into the mod that is shown, which the menu keeps as it is while this is active.
     */
    class AdjustInWorld
    {
    public:
        bool isActive() const;
        void start(const model::Setting& setting, model::Store& store);
        bool onFrameUpdate();
        void cancel();
        void stop();

    private:
        void createColumn(const model::Setting& setting);
        void addValues(std::vector<vrui::TextRow>& rows) const;
        void addControls(std::vector<vrui::TextRow>& rows) const;
        model::TuningInput readInput() const;

        const model::Setting* _setting = nullptr;
        model::Store* _store = nullptr;

        // the value it had as it started, which Cancel gives back, and the table of that, which the changes are shown from
        std::string _before;
        model::TransformTable _startTable;

        // the transform as it is tuned
        std::optional<model::TransformTuning> _tuning;

        // the time of the frame before, as a frame's time gives the tuning its speed
        std::chrono::steady_clock::time_point _lastFrame;

        // the column on the wrist, which is there while it is active
        std::shared_ptr<vrui::UIContainer> _column;

        // a press on Done or Cancel, which the frame update does: a wrist button's handler runs while the UI manager
        // iterates its elements
        bool _doneRequested = false;
        bool _cancelRequested = false;
    };
}
