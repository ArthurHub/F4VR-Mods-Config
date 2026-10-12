#include "ModPage.h"

#include <string_view>
#include <variant>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    /**
     * What a setting is, by MCM's name for how it is edited.
     */
    std::string_view typeName(const model::SettingType type)
    {
        switch (type) {
        case model::SettingType::Switcher:
            return "switcher";
        case model::SettingType::Slider:
            return "slider";
        case model::SettingType::Stepper:
            return "stepper";
        case model::SettingType::Dropdown:
            return "dropdown";
        case model::SettingType::TextInput:
            return "textinput";
        }
        return "unknown";
    }

    /**
     * The width of a row: the same whether or not the page's scroll bar shows.
     */
    float rowWidth()
    {
        return ImGui::GetWindowWidth() - theme::SCROLLBAR_SIZE - theme::SPACE_2;
    }

    /**
     * A text row at the cursor: its lines in the muted color, in from the edges of the rows and wrapped
     * between them.
     */
    void drawTextRow(const model::Text& text)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const float height = ui::drawTextLines(text.lines, ImVec2(pos.x + theme::SPACE_2, pos.y), width - theme::SPACE_2 * 2.0f, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * A row at the cursor, as its text and what it is. It is laid out as a setting's row: the text after the
     * gutter, and the type where the control ends, before the room of its Reset.
     */
    void drawRow(const std::string_view text, const std::string_view type)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        ImGui::Dummy(ImVec2(width, theme::CONTROL_HEIGHT));
        if (!ImGui::IsItemVisible()) {
            return;
        }

        const float typeLeft = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - ui::textWidth(type);
        ui::drawText(typeLeft, pos.x + width, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, type);
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, typeLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
    }

    void drawItems(const std::vector<model::Item>& items);

    /**
     * Draws an item as its row. A section is its heading, with the rows of its items under it, and a spacer
     * is empty space as high as its lines of text.
     */
    struct ItemDrawer
    {
        void operator()(const model::Setting& setting) const
        {
            drawRow(setting.text, typeName(setting.type));
        }

        void operator()(const model::Group& group) const
        {
            ui::sectionHeading(group.text);
            drawItems(group.items);
        }

        void operator()(const model::Button& button) const
        {
            drawRow(button.text, "button");
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text);
        }

        void operator()(const model::Image&) const
        {
            drawRow({}, "image");
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            drawRow(placeholder.text, placeholder.type.empty() ? "unknown" : std::string_view(placeholder.type));
        }
    };

    void drawItems(const std::vector<model::Item>& items)
    {
        for (const auto& item : items) {
            std::visit(ItemDrawer{}, item);
        }
    }
}

namespace mods_config::ui
{
    /**
     * Draws the rows of a page into the current ImGui window, which scrolls them.
     */
    void ModPage::draw(const model::Page& page) const
    {
        drawItems(page.items);
    }
}
