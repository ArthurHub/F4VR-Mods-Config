#include "AdjustInWorld.h"

#include <algorithm>
#include <format>
#include <string_view>

#include "Theme.h"
#include "common/CommonUtils.h"
#include "f4vr/F4VRUtils.h"
#include "vrcf/VRControllersManager.h"
#include "vrcf/VRControllersSuppressor.h"
#include "vrui/BindingPrompt.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"

using namespace vrui;

namespace
{
    using namespace mods_config;

    // the name the input is taken from the game under
    constexpr auto SUPPRESS_KEY = "ModsConfigFO4_AdjustInWorld";

    // the column on the wrist, in vrui units: each text panel is as wide as its content needs
    constexpr float COLUMN_GAP = 0.25f;
    constexpr float TITLE_TEXT_HEIGHT = 0.65f;
    constexpr float VALUES_TEXT_HEIGHT = 0.3f;
    constexpr float CONTROLS_TEXT_HEIGHT = 0.35f;
    constexpr float CONTROLS_SIDE_PADDING = 0.3f;

    // the controls' icons, larger than a span's default so the marks on them read, as the footers of FRIK's config screens
    constexpr BindingPromptStyle CONTROLS_ICON_STYLE{ .imageHeight = 3.0f };

    constexpr float WRIST_BUTTON_WIDTH = 3.0f;
    constexpr float WRIST_BUTTON_HEIGHT = 2.5f;
    constexpr float WRIST_BUTTON_TEXT_HEIGHT = 0.4f;
    constexpr float WRIST_BUTTON_GAP = 0.5f;

    // the icons of the wrist's buttons, under the mod's `Data\Textures\ModsConfigFO4VR`
    constexpr auto CANCEL_ICON = R"(vrui\cancel.dds)";
    constexpr auto DONE_ICON = R"(vrui\done.dds)";

    // where the column lies, from the framework's place on the wrist: as the buttons it stands in for
    const RE::NiTransform COLUMN_OFFSET = common::MatrixUtils::getTransform(0, 0, 0, 0, 0, 0);

    // the longest a frame counts as, so a hitch does not throw the object away
    constexpr float LONGEST_FRAME_SECONDS = 0.1f;

    // what a value is shown as while it differs from the start
    constexpr auto PENDING_COLOR = render::Color::rgba(theme::color::AMBER & 0xFF, (theme::color::AMBER >> 8) & 0xFF, (theme::color::AMBER >> 16) & 0xFF);

    /**
     * A thumbstick's icon at the end of a line, moved along the axes the tuning reads of it. The tuning reads the
     * controllers directly, so this binding only says which one it reads, with the hand resolved by the player's handedness.
     */
    void appendStick(std::vector<TextSpan>& spans, const vrcf::Hand hand, const vrcf::Direction moved)
    {
        appendBindingPrompt(spans, { .hand = hand, .type = vrcf::ActivationType::AxisDirection, .axis = vrcf::Axis::Thumbstick, .direction = moved }, CONTROLS_ICON_STYLE);
    }

    /**
     * The icon of a hold of a button of the offhand at the end of a line, as the controller prints it.
     */
    void appendHold(std::vector<TextSpan>& spans, const vr::EVRButtonId button)
    {
        appendBindingPrompt(spans, { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::HoldDown, .button = button }, CONTROLS_ICON_STYLE);
    }

    /**
     * Both thumbsticks at the end of a line: the primary moved any way, and the offhand along the axes it is read on.
     */
    void appendBothSticks(std::vector<TextSpan>& spans, const vrcf::Direction offhandMoved)
    {
        appendStick(spans, vrcf::Hand::Primary, vrcf::Direction::Any);
        spans.push_back({ .text = " / " });
        appendStick(spans, vrcf::Hand::Offhand, offhandMoved);
    }

    /**
     * A number of the wrist: always two decimals, so the text keeps its width as a value changes. One that rounds to zero
     * has no minus.
     */
    std::string wristNumber(const float value)
    {
        const auto text = std::format("{:.2f}", value);
        return text == "-0.00" ? "0.00" : text;
    }
}

namespace mods_config
{
    bool AdjustInWorld::isActive() const
    {
        return _setting != nullptr;
    }

    /**
     * Starts tuning a transform from its value in the store, or from its default for a value that is no transform, or an
     * empty table with none: the column is put on the wrist, and the controllers' input is taken from the game.
     */
    void AdjustInWorld::start(const model::Setting& setting, model::Store& store)
    {
        const auto stored = store.value(setting);
        const auto* text = std::get_if<std::string>(&stored.value);
        _before = text ? *text : std::string();
        auto table = model::parseTransformTable(_before);
        if (!table && setting.defaultValue) {
            table = model::parseTransformTable(std::get<std::string>(*setting.defaultValue));
        }
        _startTable = table ? *table : model::TransformTable{};

        _setting = &setting;
        _store = &store;
        _tuning.emplace(setting.valueOptions, _startTable);
        _doneRequested = false;
        _cancelRequested = false;
        _lastFrame = std::chrono::steady_clock::now();
        createColumn(setting);
        vrcf::VRControllersSuppress.setAllSuppressed(SUPPRESS_KEY, true);
        logger::info("Adjust in world: {}", setting.text);
    }

    /**
     * Tunes the transform by the controllers in this frame, and sets a change in the store. Returns true in the frame a
     * press on Done or Cancel ended it, and the panel can be shown again.
     */
    bool AdjustInWorld::onFrameUpdate()
    {
        if (_cancelRequested) {
            cancel();
            return true;
        }
        if (_doneRequested) {
            stop();
            return true;
        }
        const auto now = std::chrono::steady_clock::now();
        const float seconds = (std::min)(std::chrono::duration<float>(now - _lastFrame).count(), LONGEST_FRAME_SECONDS);
        _lastFrame = now;
        if (_tuning->update(readInput(), seconds)) {
            _store->set(*_setting, model::transformText(_tuning->table()));
        }
        return false;
    }

    /**
     * Ends the tuning and gives back the value it had as it started, so the editor's changes before it stay.
     */
    void AdjustInWorld::cancel()
    {
        if (!isActive()) {
            return;
        }
        _store->set(*_setting, _before);
        stop();
    }

    /**
     * Ends the tuning with the value as it is: the column is taken off the wrist, and the controllers are the game's.
     * Called as the menu closes too, whose session discards the value.
     */
    void AdjustInWorld::stop()
    {
        if (!isActive()) {
            return;
        }
        logger::info("Adjust in world ended");
        vrcf::VRControllersSuppress.setAllSuppressed(SUPPRESS_KEY, false);
        g_uiManager->detachElement(_column, true);
        _column.reset();
        _tuning.reset();
        _setting = nullptr;
        _store = nullptr;
    }

    /**
     * The controllers as the tuning reads them, by the primary and the offhand, so a left-handed player's are mirrored.
     */
    model::TuningInput AdjustInWorld::readInput() const
    {
        const auto [primaryX, primaryY] = vrcf::VRControllers.getThumbstickValue(vrcf::Hand::Primary);
        const auto [offhandX, offhandY] = vrcf::VRControllers.getThumbstickValue(vrcf::Hand::Offhand);
        return { .primaryX = primaryX,
            .primaryY = primaryY,
            .offhandX = offhandX,
            .offhandY = offhandY,
            .rotating = vrcf::VRControllers.isPressHeldDown(vrcf::Hand::Offhand, vr::k_EButton_Grip),
            .scaling = vrcf::VRControllers.isPressHeldDown(vrcf::Hand::Offhand, vr::k_EButton_A) };
    }

    /**
     * The column on the wrist, a column stacking its rows from the bottom up: Cancel and Done, the controls, the values,
     * and the setting's name, as the design system's Transform has it.
     */
    void AdjustInWorld::createColumn(const model::Setting& setting)
    {
        const auto cancelButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_AdjustCancelButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        cancelButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        cancelButton->setImage(CANCEL_ICON);
        cancelButton->setBottomText("CANCEL");
        cancelButton->setOnPressHandler([this](UIButtonPanel*) {
            _cancelRequested = true;
        });

        const auto doneButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_AdjustDoneButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        doneButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        doneButton->setImage(DONE_ICON);
        doneButton->setBottomText("DONE");
        doneButton->setOnPressHandler([this](UIButtonPanel*) {
            _doneRequested = true;
        });

        const auto buttons = std::make_shared<UIContainer>("ModsConfigFO4_AdjustButtonRow", UIContainerLayout::HorizontalCenter, WRIST_BUTTON_GAP);
        buttons->addElement(cancelButton);
        buttons->addElement(doneButton);

        const auto controls = std::make_shared<UITextPanel>("ModsConfigFO4_AdjustControls");
        auto controlsStyle = F4VR_PANEL_STYLE;
        controlsStyle.padding = UIPadding::symmetric(F4VR_PANEL_STYLE.padding.top, CONTROLS_SIDE_PADDING);
        controls->setStyle(controlsStyle);
        controls->setTextHeight(CONTROLS_TEXT_HEIGHT);
        controls->setContent([this](std::vector<TextRow>& rows) {
            addControls(rows);
        });

        const auto values = std::make_shared<UITextPanel>("ModsConfigFO4_AdjustValues");
        values->setStyle(F4VR_PANEL_STYLE);
        values->setTextHeight(VALUES_TEXT_HEIGHT);
        values->setContent([this](std::vector<TextRow>& rows) {
            addValues(rows);
        });

        const auto title = std::make_shared<UITextPanel>("ModsConfigFO4_AdjustTitle");
        title->setStyle(UIPanelStyle{ .color = F4VR_PANEL_STYLE.color });
        title->setTextHeight(TITLE_TEXT_HEIGHT);
        title->setContent([name = std::format("Mods Config: {}", setting.text)](std::vector<TextRow>& rows) {
            rows.emplace_back(name);
        });

        _column = std::make_shared<UIContainer>("ModsConfigFO4_AdjustColumn", UIContainerLayout::VerticalUp, COLUMN_GAP);
        _column->addElement(buttons);
        _column->addElement(controls);
        _column->addElement(values);
        _column->addElement(title);
        g_uiManager->attachPresetToOffhandWrist(_column, COLUMN_OFFSET);
    }

    /**
     * The values as they change, on one line: the parts that are on, each number in amber while it differs from the start.
     * Called by the panel, which the UI manager draws while this is active.
     */
    void AdjustInWorld::addValues(std::vector<TextRow>& rows) const
    {
        if (!_tuning) {
            return;
        }
        const auto& table = _tuning->table();
        const auto& axes = _setting->valueOptions.transformAxes;
        TextRow row;
        const auto addNumber = [&](const std::size_t axis, const std::string_view unit, const std::string_view prefix, const std::string_view separator) {
            const float value = model::axisValue(table, axis);
            auto& span = row.spans.emplace_back(TextSpan{ .text = std::format("{}{}{}", prefix, wristNumber(value), unit) });
            if (common::fNotEqual(value, model::axisValue(_startTable, axis))) {
                span.color = PENDING_COLOR;
            }
            row.spans.push_back({ .text = std::string(separator) });
        };
        constexpr std::string_view BETWEEN_NUMBERS = ", ";
        constexpr std::string_view BETWEEN_PARTS = "   ";
        for (const auto [first, unit] : { std::pair{ model::TRANSFORM_POSITION, std::string_view() }, std::pair{ model::TRANSFORM_ROTATION, std::string_view("°") } }) {
            if (axes[first].on) {
                addNumber(first, unit, "", BETWEEN_NUMBERS);
                addNumber(first + 1, unit, "", BETWEEN_NUMBERS);
                addNumber(first + 2, unit, "", BETWEEN_PARTS);
            }
        }
        if (axes[model::TRANSFORM_SCALE].on) {
            addNumber(model::TRANSFORM_SCALE, "", "×", "");
        }
        rows.push_back(std::move(row));
    }

    /**
     * The controls of the parts that are on, as the footer of FRIK's Pip-Boy adjust screen has them: the sticks, and a
     * line of what holding a modifier makes of them. With position off a modifier has its sticks on its own line: both for
     * a rotation, which the offhand's X rolls, and the primary's for a scale. Each stick's icon shows the axes it is read
     * on: the offhand's Y moves, and its X rolls a rotation.
     */
    void AdjustInWorld::addControls(std::vector<TextRow>& rows) const
    {
        if (!_setting) {
            return;
        }
        const auto& axes = _setting->valueOptions.transformAxes;
        const bool rotation = axes[model::TRANSFORM_ROTATION].on;
        const bool scale = axes[model::TRANSFORM_SCALE].on;
        if (!axes[model::TRANSFORM_POSITION].on) {
            if (rotation) {
                rows.push_back({ .spans = { { .text = "ROTATION:  " } } });
                appendHold(rows.back().spans, vr::k_EButton_Grip);
                rows.back().spans.push_back({ .text = " + " });
                appendBothSticks(rows.back().spans, vrcf::Direction::RightLeft);
            }
            if (scale) {
                rows.push_back({ .spans = { { .text = "SCALE:  " } } });
                appendHold(rows.back().spans, vr::k_EButton_A);
                rows.back().spans.push_back({ .text = " + " });
                appendStick(rows.back().spans, vrcf::Hand::Primary, vrcf::Direction::UpDown);
            }
            return;
        }
        rows.push_back({ .spans = { { .text = "ADJUST:  " } } });
        appendBothSticks(rows.back().spans, rotation ? vrcf::Direction::Any : vrcf::Direction::UpDown);
        if (rotation || scale) {
            rows.push_back({});
        }
        if (rotation) {
            rows.back().spans.push_back({ .text = "+ROTATION: " });
            appendHold(rows.back().spans, vr::k_EButton_Grip);
        }
        if (scale) {
            rows.back().spans.push_back({ .text = rotation ? "  +SCALE: " : "+SCALE: " });
            appendHold(rows.back().spans, vr::k_EButton_A);
        }
    }
}
