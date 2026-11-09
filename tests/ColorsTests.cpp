#include <catch2/catch_test_macros.hpp>

#include <string>

#include "model/Colors.h"
#include "model/ConfigDefinition.h"

using namespace mods_config;

TEST_CASE("Colors: an INI line is r,g,b or r,g,b,a, with spaces around a number")
{
    const auto rgb = model::parseColorTable("255,180,107", model::TableForm::IniLine);
    REQUIRE(rgb);
    CHECK(*rgb == model::ColorTable{ .rgb = std::array{ 255, 180, 107 }, .alpha = {} });

    const auto rgba = model::parseColorTable(" 190 , 255,190, 0 ", model::TableForm::IniLine);
    REQUIRE(rgba);
    CHECK(*rgba == model::ColorTable{ .rgb = std::array{ 190, 255, 190 }, .alpha = 0 });
}

TEST_CASE("Colors: an INI line that is no color has the reason, and an empty one is none")
{
    const std::string reason = "it is not r,g,b or r,g,b,a, each a whole number in 0..255, as 255,180,107";
    for (const auto* line : { "", "255,180", "1,2,3,4,5", "256,0,0", "-1,0,0", "1.5,0,0", "a,b,c", "1,,3", "1 2 3", "{ rgb = [1, 2, 3] }" }) {
        INFO(line);
        CHECK(model::parseColorTable(line, model::TableForm::IniLine).error() == reason);
    }
}

TEST_CASE("Colors: an INI line is written with no spaces, white for no rgb, and an alpha where the table has one")
{
    CHECK(model::colorText({ .rgb = std::array{ 190, 255, 190 }, .alpha = {} }, model::TableForm::IniLine) == "190,255,190");
    CHECK(model::colorText({ .rgb = std::array{ 190, 255, 190 }, .alpha = 200 }, model::TableForm::IniLine) == "190,255,190,200");
    CHECK(model::colorText({ .rgb = {}, .alpha = 10 }, model::TableForm::IniLine) == "255,255,255,10");

    // two lines of one color are one line, and one that is none is as it is
    CHECK(model::canonicalColor(" 1, 2 ,3 ", model::TableForm::IniLine) == "1,2,3");
    CHECK(model::canonicalColor("1,2", model::TableForm::IniLine) == "1,2");
}

TEST_CASE("Colors: a line with no alpha is read over its default's alpha")
{
    model::Setting setting{ .type = model::SettingType::Color };
    setting.tableForm = model::TableForm::IniLine;
    setting.defaultValue = "190,255,190,200";
    const auto color = model::colorOf(setting, std::string("255,0,0"));
    REQUIRE(color);
    CHECK(color->bytes() == std::array{ 255, 0, 0, 200 });
    CHECK_FALSE(model::colorOf(setting, std::string("{ rgb = [255, 0, 0] }")));
}
