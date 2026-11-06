#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * The row the ray is on: what the row says, with the states of its mod's client in it, and its help. A row whose
     * value is not available has the reason as its help, and one whose value is invalid has why as its help,
     * which is an error. A row that warns has the text of its warning, the model's own too. A row whose action
     * gave a result has it, which is told before its help. A setting whose change waits for a Save has when it
     * takes effect.
     */
    struct PointedRow
    {
        std::string text;
        std::string help;
        std::string_view warning;
        bool invalid = false;
        std::string_view result = {};
        std::optional<model::Applies> applies = {};
    };

    /**
     * How the status of the help line looks: plain in the muted color, in amber after the pending mark while
     * changes wait for a Save, in red after the invalid mark while a change is invalid and for a Save that
     * failed, or in green after the Restart mark for a Save that wrote a change that takes effect after a
     * restart.
     */
    enum class StatusLook : std::uint8_t
    {
        Plain,
        Pending,
        Invalid,
        Restart,
    };

    /**
     * The help line, on the left of the Full panel's footer: the help of the row the ray is on, as
     * `Name: help`, after the tag of when a pending change takes effect where it is known, with the warning of a row that warns before
     * its help, and why a row is invalid in red after the invalid mark. It keeps the row it was last given, so the line does not change while the ray is in the
     * gap between two rows. With no row it has the status of the editing session, where there is one.
     */
    class HelpLine
    {
    public:
        void show(const PointedRow& row);
        void clear();
        void setStatus(std::string_view status, StatusLook look = StatusLook::Plain);
        void draw(const ImVec2& at, float lineWidth) const;
        std::string text() const;

        const std::string& status() const
        {
            return _status;
        }

        StatusLook statusLook() const
        {
            return _look;
        }

        bool isInvalid() const
        {
            return _invalid;
        }

        std::string_view tag() const;

    private:
        // the row's text, with the colon and the space that stand before a help, the row's warning, with the
        // space before a help, and the row's help
        std::string _name;
        std::string _warning;
        std::string _help;

        // the result of the row's action, with the space before a help
        std::string _result;

        // whether the row's help is why its value is invalid
        bool _invalid = false;

        // when the change of the row's setting takes effect, for a setting that is set
        std::optional<model::Applies> _applies;

        // what the line says with no row: empty for nothing
        std::string _status;

        // how the status looks
        StatusLook _look = StatusLook::Plain;
    };
}
