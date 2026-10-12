#include <catch2/catch_test_macros.hpp>

#include "model/Translations.h"

using namespace mods_config;

TEST_CASE("Translations: a token becomes the text of the language")
{
    model::Translations translations;
    translations.add("$NotFound", "\"Not Found\" notifications");
    translations.add("$Empty", "");

    CHECK(translations.size() == 2);
    CHECK(translations.translate("$NotFound") == "\"Not Found\" notifications");
    // a token's text can be empty
    CHECK(translations.translate("$Empty").empty());
}

TEST_CASE("Translations: a text that is not a token of the language stays as it is written")
{
    model::Translations translations;
    translations.add("$NotFound", "Not found");
    // a file's line that has no `$` is never looked up
    translations.add("Enabled", "On");

    CHECK(translations.translate("Enabled") == "Enabled");
    CHECK(translations.translate("").empty());
    CHECK(translations.translate("$Missing") == "$Missing");
    // a token is the whole text
    CHECK(translations.translate("$NotFound now") == "$NotFound now");
    CHECK(translations.translate("See $NotFound") == "See $NotFound");
    CHECK(model::Translations().translate("$NotFound") == "$NotFound");
}

TEST_CASE("Translations: the first text of a token is kept")
{
    model::Translations translations;
    translations.add("$Name", "First");
    translations.add("$Name", "Second");

    CHECK(translations.size() == 1);
    CHECK(translations.translate("$Name") == "First");
}
