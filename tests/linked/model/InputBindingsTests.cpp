#include <catch2/catch_test_macros.hpp>

#include <string>

#include "model/ConfigDefinition.h"
#include "model/InputBindings.h"

using namespace mods_config;

namespace
{
    /**
     * The INI line of a binding's line read and written again, or the reason it is none.
     */
    std::string rewritten(const std::string_view line)
    {
        const auto binding = model::parseBinding(line, model::TableForm::IniLine);
        return binding ? model::bindingText(*binding, model::TableForm::IniLine) : binding.error();
    }
}

TEST_CASE("InputBindings: an INI line is read with the other names of its words, and written in the grammar's own")
{
    CHECK(rewritten("off tap trigger") == "offhand tap trigger");
    CHECK(rewritten("main click b") == "primary press menu");
    CHECK(rewritten("primary longpress thumbstick 1.5 +offhand:thumbstick") == "primary longpress thumbstick 1.5 +offhand:thumbstick");
    CHECK(rewritten("left tap grip suppress +a") == "left tap grip suppress +a");
    CHECK(rewritten("primary thumbstick up") == "primary axis thumbstick up");
    CHECK(rewritten("offhand axis trigger updown 0.5") == "offhand axis trigger updown 0.5");
    CHECK(rewritten("any tap grip nosuppress") == "any tap grip");
}

TEST_CASE("InputBindings: an off binding is none, and an empty INI line is off")
{
    CHECK(rewritten("") == "none");
    CHECK(rewritten("disabled") == "none");
    CHECK(rewritten("none") == "none");
}

TEST_CASE("InputBindings: an INI line that is no binding has the framework's reason, with its first letter in upper case")
{
    const auto typo = model::parseBinding("offhand tap thumbstik", model::TableForm::IniLine);
    REQUIRE_FALSE(typo);
    CHECK(typo.error().starts_with("'thumbstik' is not a button"));
    const auto missing = model::parseBinding("offhand", model::TableForm::IniLine);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().starts_with("The type is missing"));
    CHECK_FALSE(model::parseBinding(R"({ hand = "offhand", type = "tap", button = "grip" })", model::TableForm::IniLine));
}

TEST_CASE("InputBindings: a binding's table and its INI line are one binding")
{
    const auto table = model::parseBinding(R"({ hand = "primary", type = "longpress", button = "thumbstick", duration = 1.5, modifier = { hand = "offhand", button = "grip" } })",
        model::TableForm::Toml);
    REQUIRE(table);
    const auto line = model::bindingText(*table, model::TableForm::IniLine);
    CHECK(line == "primary longpress thumbstick 1.5 +offhand:grip");
    const auto back = model::parseBinding(line, model::TableForm::IniLine);
    REQUIRE(back);
    CHECK(model::bindingText(*back, model::TableForm::Toml) == model::bindingText(*table, model::TableForm::Toml));
}
