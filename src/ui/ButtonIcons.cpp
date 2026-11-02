#include "ButtonIcons.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include <imgui.h>
#include <imgui_internal.h>

namespace
{
    using namespace mods_config;

    // the strokes of a button's icon, in its box of 20
    constexpr float STROKE = 2.0f;

    /**
     * Draws the shapes of an icon in its box, by the coordinates of the design system's paths. ImGui's strokes have
     * square ends and joins, so an opaque stroke has a dot on each of its points, which makes them round.
     */
    class Pen
    {
    public:
        Pen(const ImVec2& center, const ImU32 color)
            : _center(center),
              _color(ImGui::GetColorU32(color)),
              _drawList(ImGui::GetWindowDrawList())
        {}

        void lineTo(const float x, const float y) const
        {
            _drawList->PathLineTo(at(x, y));
        }

        void curveTo(const float controlX, const float controlY, const float x, const float y) const
        {
            _drawList->PathBezierQuadraticCurveTo(at(controlX, controlY), at(x, y));
        }

        /**
         * An arc from the end of the path to a point, clockwise and the short way, as SVG's `A r r 0 0 1`: a radius
         * too small to reach the point is made as large as half the way.
         */
        void arcTo(const float x, const float y, const float radius) const
        {
            const auto from = _drawList->_Path.back();
            const auto to = at(x, y);
            const ImVec2 half((to.x - from.x) * 0.5f, (to.y - from.y) * 0.5f);
            const float length = std::hypot(half.x, half.y);
            const float rise = std::sqrt((std::max)(radius * radius - length * length, 0.0f));
            const ImVec2 center(from.x + half.x - half.y / length * rise, from.y + half.y + half.x / length * rise);
            const float start = std::atan2(from.y - center.y, from.x - center.x);
            const float end = std::atan2(to.y - center.y, to.x - center.x);
            // the arc starts with this point again, and a stroke makes a spike at two points that are one
            _drawList->_Path.pop_back();
            _drawList->PathArcTo(center, (std::max)(radius, length), start, end < start ? end + IM_PI * 2.0f : end);
        }

        void stroke(const ImDrawFlags flags = ImDrawFlags_None) const
        {
            // a filled circle draws through the path, so the dots wait for the stroke. A faded stroke has none, as
            // a dot over it would show
            const auto points = _drawList->_Path;
            _drawList->PathStroke(_color, flags, STROKE);
            if ((_color & IM_COL32_A_MASK) != IM_COL32_A_MASK) {
                return;
            }
            for (const auto& point : points) {
                _drawList->AddCircleFilled(point, STROKE * 0.5f, _color);
            }
        }

        /**
         * Fills the path, whose points have to go clockwise: ImGui's soft edge is on the wrong side of any other.
         */
        void fill() const
        {
            _drawList->PathFillConcave(_color);
        }

        void line(const std::initializer_list<ImVec2> points) const
        {
            for (const auto& point : points) {
                lineTo(point.x, point.y);
            }
            stroke();
        }

        void rect(const float left, const float top, const float right, const float bottom, const float rounding) const
        {
            _drawList->PathRect(at(left, top), at(right, bottom), rounding);
            stroke(ImDrawFlags_Closed);
        }

        /**
         * The outline of a circle. ImGui draws its circles half a pixel smaller than their radius.
         */
        void circle(const float x, const float y, const float radius) const
        {
            _drawList->AddCircle(at(x, y), radius + 0.5f, _color, 0, STROKE);
        }

        void disc(const float x, const float y, const float radius) const
        {
            _drawList->AddCircleFilled(at(x, y), radius, _color);
        }

    private:
        ImVec2 at(const float x, const float y) const
        {
            return { _center.x + x - ui::BUTTON_ICON_SIZE * 0.5f, _center.y + y - ui::BUTTON_ICON_SIZE * 0.5f };
        }

        ImVec2 _center;
        ImU32 _color;
        ImDrawList* _drawList;
    };
}

namespace mods_config::ui
{
    /**
     * A button's icon around a point, in a color, as the design system's ButtonRow draws it in a box of 20: 2 px
     * strokes with round ends and joins, or a fill. Nothing for none. Inside `BeginDisabled` it is faded.
     */
    void drawButtonIcon(const model::ButtonIcon icon, const ImVec2& center, const ImU32 color)
    {
        const Pen pen(center, color);
        switch (icon) {
        case model::ButtonIcon::None:
            break;
        case model::ButtonIcon::Play:
            pen.lineTo(5.0f, 3.0f);
            pen.lineTo(16.0f, 10.0f);
            pen.lineTo(5.0f, 17.0f);
            pen.fill();
            break;
        case model::ButtonIcon::Stop:
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(center.x - 5.5f, center.y - 5.5f), ImVec2(center.x + 5.5f, center.y + 5.5f), ImGui::GetColorU32(color), 1.5f);
            break;
        case model::ButtonIcon::Bolt:
            pen.lineTo(11.0f, 2.0f);
            pen.lineTo(11.0f, 8.0f);
            pen.lineTo(16.0f, 8.0f);
            pen.lineTo(8.0f, 18.0f);
            pen.lineTo(9.0f, 11.0f);
            pen.lineTo(4.0f, 11.0f);
            pen.fill();
            break;
        case model::ButtonIcon::Target:
            pen.circle(10.0f, 10.0f, 6.5f);
            pen.disc(10.0f, 10.0f, 2.0f);
            pen.line({ { 10.0f, 1.0f }, { 10.0f, 4.0f } });
            pen.line({ { 10.0f, 16.0f }, { 10.0f, 19.0f } });
            pen.line({ { 1.0f, 10.0f }, { 4.0f, 10.0f } });
            pen.line({ { 16.0f, 10.0f }, { 19.0f, 10.0f } });
            break;
        case model::ButtonIcon::Check:
            pen.line({ { 3.5f, 10.5f }, { 8.0f, 15.0f }, { 16.5f, 5.5f } });
            break;
        case model::ButtonIcon::Window:
            pen.rect(2.5f, 3.5f, 17.5f, 16.5f, 1.5f);
            pen.line({ { 2.5f, 7.5f }, { 17.5f, 7.5f } });
            pen.line({ { 5.0f, 5.5f }, { 6.0f, 5.5f } });
            pen.line({ { 7.5f, 5.5f }, { 8.5f, 5.5f } });
            break;
        case model::ButtonIcon::External:
            pen.line({ { 11.0f, 3.0f }, { 17.0f, 3.0f }, { 17.0f, 9.0f } });
            pen.line({ { 17.0f, 3.0f }, { 9.0f, 11.0f } });
            pen.line({ { 14.0f, 12.0f }, { 14.0f, 17.0f }, { 3.0f, 17.0f }, { 3.0f, 6.0f }, { 8.0f, 6.0f } });
            break;
        case model::ButtonIcon::Link:
            pen.line({ { 8.5f, 11.5f }, { 11.5f, 8.5f } });
            pen.lineTo(9.5f, 5.5f);
            pen.lineTo(11.0f, 4.0f);
            pen.arcTo(15.5f, 8.5f, 3.2f);
            pen.lineTo(14.0f, 10.0f);
            pen.stroke();
            pen.lineTo(10.5f, 14.5f);
            pen.lineTo(9.0f, 16.0f);
            pen.arcTo(4.5f, 11.5f, 3.2f);
            pen.lineTo(6.0f, 10.0f);
            pen.stroke();
            break;
        case model::ButtonIcon::Gear:
            pen.circle(10.0f, 10.0f, 2.6f);
            pen.circle(10.0f, 10.0f, 5.5f);
            pen.line({ { 10.0f, 2.5f }, { 10.0f, 4.5f } });
            pen.line({ { 10.0f, 15.5f }, { 10.0f, 17.5f } });
            pen.line({ { 2.5f, 10.0f }, { 4.5f, 10.0f } });
            pen.line({ { 15.5f, 10.0f }, { 17.5f, 10.0f } });
            pen.line({ { 4.7f, 4.7f }, { 6.1f, 6.1f } });
            pen.line({ { 13.9f, 13.9f }, { 15.3f, 15.3f } });
            pen.line({ { 4.7f, 15.3f }, { 6.1f, 13.9f } });
            pen.line({ { 13.9f, 6.1f }, { 15.3f, 4.7f } });
            break;
        case model::ButtonIcon::Import:
            pen.line({ { 10.0f, 3.0f }, { 10.0f, 12.0f } });
            pen.line({ { 6.0f, 8.5f }, { 10.0f, 12.5f }, { 14.0f, 8.5f } });
            pen.line({ { 3.5f, 13.5f }, { 3.5f, 16.5f }, { 16.5f, 16.5f }, { 16.5f, 13.5f } });
            break;
        case model::ButtonIcon::Export:
            pen.line({ { 10.0f, 13.0f }, { 10.0f, 3.5f } });
            pen.line({ { 6.0f, 7.0f }, { 10.0f, 3.0f }, { 14.0f, 7.0f } });
            pen.line({ { 3.5f, 13.5f }, { 3.5f, 16.5f }, { 16.5f, 16.5f }, { 16.5f, 13.5f } });
            break;
        case model::ButtonIcon::Copy:
            pen.rect(6.5f, 6.5f, 16.5f, 16.5f, 1.5f);
            pen.line({ { 13.5f, 4.0f }, { 13.5f, 3.5f }, { 3.5f, 3.5f }, { 3.5f, 13.5f }, { 4.0f, 13.5f } });
            break;
        case model::ButtonIcon::Eye:
            pen.lineTo(1.5f, 10.0f);
            pen.curveTo(10.0f, 2.0f, 18.5f, 10.0f);
            pen.curveTo(10.0f, 18.0f, 1.5f, 10.0f);
            pen.stroke();
            pen.disc(10.0f, 10.0f, 2.5f);
            break;
        case model::ButtonIcon::Hand:
            pen.lineTo(6.0f, 11.0f);
            pen.lineTo(6.0f, 5.0f);
            pen.arcTo(8.6f, 5.0f, 1.3f);
            pen.lineTo(8.6f, 9.0f);
            pen.stroke();
            pen.lineTo(8.6f, 9.0f);
            pen.lineTo(8.6f, 3.8f);
            pen.arcTo(11.2f, 3.8f, 1.3f);
            pen.lineTo(11.2f, 9.0f);
            pen.stroke();
            pen.lineTo(11.2f, 9.0f);
            pen.lineTo(11.2f, 4.8f);
            pen.arcTo(13.8f, 4.8f, 1.3f);
            pen.lineTo(13.8f, 10.0f);
            pen.stroke();
            pen.lineTo(13.8f, 10.0f);
            pen.lineTo(13.8f, 7.5f);
            pen.arcTo(16.4f, 7.5f, 1.3f);
            pen.lineTo(16.4f, 12.0f);
            pen.curveTo(16.4f, 17.5f, 11.0f, 17.5f);
            pen.curveTo(7.5f, 17.5f, 5.5f, 14.5f);
            pen.lineTo(3.4f, 11.4f);
            pen.arcTo(5.6f, 10.0f, 1.3f);
            pen.lineTo(6.0f, 11.0f);
            pen.stroke();
            break;
        case model::ButtonIcon::Sound:
            pen.lineTo(3.0f, 7.5f);
            pen.lineTo(6.0f, 7.5f);
            pen.lineTo(10.5f, 4.0f);
            pen.lineTo(10.5f, 16.0f);
            pen.lineTo(6.0f, 12.5f);
            pen.lineTo(3.0f, 12.5f);
            pen.fill();
            pen.lineTo(13.5f, 7.0f);
            pen.curveTo(15.5f, 10.0f, 13.5f, 13.0f);
            pen.stroke();
            pen.lineTo(15.5f, 5.0f);
            pen.curveTo(19.0f, 10.0f, 15.5f, 15.0f);
            pen.stroke();
            break;
        case model::ButtonIcon::Pin:
            pen.lineTo(10.0f, 18.0f);
            pen.curveTo(4.0f, 11.5f, 4.0f, 8.0f);
            pen.arcTo(16.0f, 8.0f, 6.0f);
            pen.curveTo(16.0f, 11.5f, 10.0f, 18.0f);
            pen.stroke();
            pen.circle(10.0f, 8.0f, 2.0f);
            break;
        case model::ButtonIcon::Refresh:
            pen.lineTo(4.0f, 8.0f);
            pen.arcTo(15.5f, 6.0f, 6.5f);
            pen.stroke();
            pen.line({ { 16.0f, 2.5f }, { 16.0f, 6.5f }, { 12.0f, 6.5f } });
            pen.lineTo(16.0f, 12.0f);
            pen.arcTo(4.5f, 14.0f, 6.5f);
            pen.stroke();
            pen.line({ { 4.0f, 17.5f }, { 4.0f, 13.5f }, { 8.0f, 13.5f } });
            break;
        case model::ButtonIcon::Trash:
            pen.line({ { 3.5f, 5.5f }, { 16.5f, 5.5f } });
            pen.line({ { 8.0f, 5.5f }, { 8.0f, 3.5f }, { 12.0f, 3.5f }, { 12.0f, 5.5f } });
            pen.line({ { 5.5f, 5.5f }, { 6.5f, 17.0f }, { 13.5f, 17.0f }, { 14.5f, 5.5f } });
            break;
        }
    }
}
