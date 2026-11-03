#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "common/CommonUtils.h"
#include "model/ConfigDefinition.h"
#include "model/TransformTuning.h"

using namespace mods_config;
using Catch::Approx;

namespace
{
    /**
     * The options of a transform with the parts that are on.
     */
    model::ValueOptions partsOn(const bool position = true, const bool rotation = true, const bool scale = true)
    {
        model::ValueOptions options;
        for (std::size_t axis = 0; axis < model::TRANSFORM_AXES; ++axis) {
            options.transformAxes[axis] = { .min = -1.0f,
                .max = 1.0f,
                .step = 1.0f,
                .on = axis < model::TRANSFORM_ROTATION ? position
                      : axis < model::TRANSFORM_SCALE  ? rotation
                                                       : scale };
        }
        return options;
    }

    // a full push of a stick up and to the right, and a second of tuning
    constexpr float FULL = 1.0f;
    constexpr float SECOND = 1.0f;
}

TEST_CASE("TransformTuning: the sticks move a transform, past the deadzone, by the time of the frame, and past the sliders' ranges")
{
    model::TransformTuning tuning(partsOn(), { .pos = std::array{ 1.0f, 2.0f, 3.0f }, .rot = {}, .scale = {} });

    // a stick that rests does nothing
    CHECK_FALSE(tuning.update({ .primaryX = 0.4f, .primaryY = -0.4f, .offhandY = 0.5f }, SECOND));
    CHECK(model::transformText(tuning.table()) == "{ pos = [1.0, 2.0, 3.0] }");

    // the primary's Y along X, its X along Y and the offhand's Y along Z, at a full push
    CHECK(tuning.update({ .primaryY = FULL }, SECOND));
    CHECK(model::axisValue(tuning.table(), 0) == Approx(14.5f));
    CHECK(tuning.update({ .primaryX = -FULL }, SECOND));
    CHECK(model::axisValue(tuning.table(), 1) == Approx(-11.5f));
    CHECK(tuning.update({ .offhandY = FULL }, SECOND));
    CHECK(model::axisValue(tuning.table(), 2) == Approx(16.5f));

    // half way past the deadzone is half the speed, for half the time
    CHECK(tuning.update({ .primaryY = 0.75f }, 0.5f));
    CHECK(model::axisValue(tuning.table(), 0) == Approx(17.88f));

    // a move adds only its own part's key
    CHECK_FALSE(tuning.table().rot);
    CHECK_FALSE(tuning.table().scale);
}

TEST_CASE("TransformTuning: a slow push adds up, though a table has two decimals")
{
    model::TransformTuning tuning(partsOn(), { .pos = std::array{ 0.0f, 0.0f, 0.0f }, .rot = {}, .scale = {} });
    int changes = 0;
    for (int frame = 0; frame < 100; ++frame) {
        changes += tuning.update({ .primaryY = FULL }, 0.0001f) ? 1 : 0;
    }
    CHECK(changes > 0);
    CHECK(model::axisValue(tuning.table(), 0) == Approx(0.14f).margin(0.01f));
}

TEST_CASE("TransformTuning: with the grip held the sticks turn the rotation's matrix, and adds its key alone")
{
    model::TransformTuning tuning(partsOn(), { .pos = {}, .rot = {}, .scale = 1.5f });

    // one axis of a transform with no rotation is that angle: the primary's Y turns the heading the other way
    CHECK(tuning.update({ .primaryY = -FULL, .rotating = true }, SECOND));
    CHECK(model::axisValue(tuning.table(), model::TRANSFORM_ROTATION) == Approx(22.5f));
    CHECK(model::axisValue(tuning.table(), model::TRANSFORM_ROTATION + 1) == Approx(0.0f).margin(0.01f));
    CHECK(model::axisValue(tuning.table(), model::TRANSFORM_ROTATION + 2) == Approx(0.0f).margin(0.01f));
    CHECK_FALSE(tuning.table().pos);
    CHECK(tuning.table().scale == 1.5f);

    // a second turn is around a fixed axis, so it changes the angles the first left at zero
    const auto after = tuning.table();
    CHECK(tuning.update({ .offhandX = FULL, .rotating = true }, SECOND));
    CHECK(tuning.table() != after);

    // nothing pushed changes nothing, though the grip is held
    const auto held = tuning.table();
    CHECK_FALSE(tuning.update({ .rotating = true }, SECOND));
    CHECK(tuning.table() == held);
}

TEST_CASE("TransformTuning: with the offhand A held the primary stick scales it, and it stops above 0")
{
    model::TransformTuning tuning(partsOn(), {});
    CHECK(tuning.update({ .primaryY = FULL, .scaling = true }, SECOND));
    CHECK(model::axisValue(tuning.table(), model::TRANSFORM_SCALE) == Approx(1.23f));
    CHECK_FALSE(tuning.table().pos);

    for (int second = 0; second < 10; ++second) {
        tuning.update({ .primaryY = -FULL, .scaling = true }, SECOND);
    }
    CHECK(model::axisValue(tuning.table(), model::TRANSFORM_SCALE) == Approx(0.05f));
    CHECK_FALSE(tuning.update({ .primaryY = -FULL, .scaling = true }, SECOND));
}

TEST_CASE("TransformTuning: a part that is off does not change, and its modifier moves nothing else")
{
    const model::TransformTable start{ .pos = std::array{ 1.0f, 2.0f, 3.0f }, .rot = std::array{ 4.0f, 5.0f, 6.0f }, .scale = 1.5f };

    model::TransformTuning noPosition(partsOn(false, true, true), start);
    CHECK_FALSE(noPosition.update({ .primaryX = FULL, .primaryY = FULL, .offhandY = FULL }, SECOND));

    model::TransformTuning noRotation(partsOn(true, false, true), start);
    CHECK_FALSE(noRotation.update({ .primaryX = FULL, .primaryY = FULL, .offhandX = FULL, .rotating = true }, SECOND));

    model::TransformTuning noScale(partsOn(true, true, false), start);
    CHECK_FALSE(noScale.update({ .primaryY = FULL, .scaling = true }, SECOND));

    CHECK(noPosition.table() == start);
    CHECK(noRotation.table() == start);
    CHECK(noScale.table() == start);
}
