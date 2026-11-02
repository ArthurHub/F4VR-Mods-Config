#include <catch2/catch_test_macros.hpp>

#include <string>

#include "common/CommonUtils.h"
#include "model/ConfigDefinition.h"
#include "model/Transforms.h"

using namespace mods_config;

TEST_CASE("Transforms: a table has the keys of its text, as the framework reads them")
{
    const auto full = model::parseTransformTable("{ scale = 1.5, rot = [20.72, 0, 90], pos = [9.37, -5.86, -5.13] }");
    REQUIRE(full);
    CHECK(full->pos == std::array{ 9.37f, -5.86f, -5.13f });
    CHECK(full->rot == std::array{ 20.72f, 0.0f, 90.0f });
    CHECK(full->scale == 1.5f);

    const auto rotation = model::parseTransformTable("{ rot = [0.0, -5.0, 0.0] }");
    REQUIRE(rotation);
    CHECK_FALSE(rotation->pos);
    CHECK_FALSE(rotation->scale);

    const auto none = model::parseTransformTable("{}");
    REQUIRE(none);
    CHECK(*none == model::TransformTable{});
}

TEST_CASE("Transforms: a text that is no transform has the framework's reason")
{
    CHECK(model::parseTransformTable("{ pos = [0, 0], scale = 1 }").error() == "pos is not an array of three numbers");
    CHECK(model::parseTransformTable("{ rot = 5 }").error() == "rot is not an array of three numbers");
    CHECK(model::parseTransformTable("{ scale = [1] }").error() == "scale is not a number");
    CHECK(model::parseTransformTable("{ size = 1 }").error() == "it has the key 'size', which is not pos, rot or scale");
    CHECK(model::parseTransformTable("5").error() == "it is not a table");
    CHECK(model::parseTransformTable("{ pos").error() == "it is not a table, as { pos = [0, 0, 0], rot = [0, 0, 0], scale = 1 }");
    CHECK(model::parseTransformTable("{}\nother = 1").error() == "it is not a table, as { pos = [0, 0, 0], rot = [0, 0, 0], scale = 1 }");
}

TEST_CASE("Transforms: a table's text has the keys it has, pos, rot and scale, with .0 on a whole number")
{
    CHECK(model::transformText({}) == "{}");
    CHECK(model::transformText({ .pos = {}, .rot = {}, .scale = 2.0f }) == "{ scale = 2.0 }");
    CHECK(model::transformText({ .pos = {}, .rot = std::array{ 0.0f, -5.0f, 0.0f }, .scale = {} }) == "{ rot = [0.0, -5.0, 0.0] }");
    CHECK(model::transformText({ .pos = std::array{ 9.37f, -5.86f, -5.13f }, .rot = std::array{ 20.72f, 0.0f, 90.0f }, .scale = 1.5f }) ==
          "{ pos = [9.37, -5.86, -5.13], rot = [20.72, 0.0, 90.0], scale = 1.5 }");

    // two texts of one transform are one text, and a text that is none is as it is
    CHECK(model::canonicalTransform("{scale=1.5,pos=[1,2,3]}") == "{ pos = [1.0, 2.0, 3.0], scale = 1.5 }");
    CHECK(model::canonicalTransform("{ pos = [0, 0] }") == "{ pos = [0, 0] }");
}

TEST_CASE("Transforms: a key the table leaves out is no move, no rotation and a scale of 1")
{
    const model::TransformTable none;
    for (std::size_t axis = 0; axis < model::TRANSFORM_SCALE; ++axis) {
        CHECK(model::axisValue(none, axis) == 0.0f);
    }
    CHECK(model::axisValue(none, model::TRANSFORM_SCALE) == 1.0f);

    const model::TransformTable full{ .pos = std::array{ 1.0f, 2.0f, 3.0f }, .rot = std::array{ 4.0f, 5.0f, 6.0f }, .scale = 7.0f };
    for (std::size_t axis = 0; axis < model::TRANSFORM_AXES; ++axis) {
        CHECK(model::axisValue(full, axis) == static_cast<float>(axis + 1));
    }
}

TEST_CASE("Transforms: a change adds the key of its part, and is written with no noise of a float")
{
    model::TransformTable table{ .pos = {}, .rot = std::array{ 0.0f, -5.0f, 0.0f }, .scale = {} };

    // the keys it had, and the key of the part that changed
    model::setAxis(table, 1, 1.0f);
    CHECK(model::transformText(table) == "{ pos = [0.0, 1.0, 0.0], rot = [0.0, -5.0, 0.0] }");
    model::setAxis(table, model::TRANSFORM_ROTATION + 2, 90.0f);
    CHECK(model::transformText(table) == "{ pos = [0.0, 1.0, 0.0], rot = [0.0, -5.0, 90.0] }");
    model::setAxis(table, model::TRANSFORM_SCALE, 1.5f);
    CHECK(model::transformText(table) == "{ pos = [0.0, 1.0, 0.0], rot = [0.0, -5.0, 90.0], scale = 1.5 }");

    // what a slider's step of 0.01 from -20 gives in floats
    model::setAxis(table, 0, -5.8600003f);
    CHECK(model::transformText(table) == "{ pos = [-5.86, 1.0, 0.0], rot = [0.0, -5.0, 90.0], scale = 1.5 }");
}
