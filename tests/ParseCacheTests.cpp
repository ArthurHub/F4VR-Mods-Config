#include <catch2/catch_test_macros.hpp>

#include <format>
#include <string>
#include <string_view>

#include "model/ParseCache.h"
#include "model/Transforms.h"

using namespace mods_config;

namespace
{
    int g_parses = 0;

    /**
     * A parse that counts its calls, and gives the text's length.
     */
    std::size_t countedLength(const std::string_view text)
    {
        ++g_parses;
        return text.size();
    }
}

TEST_CASE("ParseCache: a text is parsed once, and the cache is emptied when it is full")
{
    g_parses = 0;
    CHECK(model::cachedParse<countedLength>("abc") == 3);
    CHECK(model::cachedParse<countedLength>("abc") == 3);
    CHECK(g_parses == 1);
    CHECK(model::cachedParse<countedLength>("abcd") == 4);
    CHECK(g_parses == 2);

    // a dragged value is a new text in each frame: the texts past the limit empty it, and "abc" is parsed again
    for (int i = 0; i < 300; ++i) {
        std::ignore = model::cachedParse<countedLength>(std::format("text {}", i));
    }
    g_parses = 0;
    CHECK(model::cachedParse<countedLength>("abc") == 3);
    CHECK(g_parses == 1);
}

TEST_CASE("ParseCache: a text that is no table keeps its reason")
{
    const auto first = model::parseTransformTable("{ rot = 5 }");
    const auto again = model::parseTransformTable("{ rot = 5 }");
    REQUIRE_FALSE(first);
    REQUIRE_FALSE(again);
    CHECK(first.error() == again.error());
}
