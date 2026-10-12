#include <catch2/catch_test_macros.hpp>

#include <format>

#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    using Lines = std::vector<std::string>;

    /**
     * The lines as texts, so that a whole text is compared in one check. A run that has a size
     * or a color says so before its text, as "[30 #FFFFFF]", and so does a line that is not at the left.
     */
    Lines describe(const std::vector<model::TextLine>& lines)
    {
        Lines described;
        for (const auto& line : lines) {
            std::string text = line.align == model::TextAlign::Center ? "center: " : line.align == model::TextAlign::Right ? "right: " : "";
            for (const auto& run : line.runs) {
                std::string look = run.size ? std::to_string(*run.size) : "";
                if (run.color) {
                    look += std::format("{}#{:06X}", look.empty() ? "" : " ", *run.color);
                }
                text += look.empty() ? run.text : "[" + look + "]" + run.text;
            }
            described.push_back(std::move(text));
        }
        return described;
    }

    Lines parsed(const std::string_view text)
    {
        return describe(model::parseMarkup(text));
    }
}

TEST_CASE("TextMarkup: a text with no tags is one line")
{
    CHECK(parsed("Stand straight and look ahead.") == Lines{ "Stand straight and look ahead." });
    CHECK(parsed("").empty());
}

TEST_CASE("TextMarkup: a br starts a new line, however it is written")
{
    CHECK(parsed("One<br>Two<br/>Three<br />Four<BR>Five") == Lines{ "One", "Two", "Three", "Four", "Five" });
    // two of them leave an empty line
    CHECK(parsed("One<br><br>Two") == Lines{ "One", "", "Two" });
}

TEST_CASE("TextMarkup: a line end of the text starts a new line")
{
    CHECK(parsed("One\nTwo\r\nThree") == Lines{ "One", "Two", "Three" });
}

TEST_CASE("TextMarkup: a paragraph is on lines of its own")
{
    CHECK(parsed("Before<p>Inside</p>After") == Lines{ "Before", "Inside", "After" });
    CHECK(parsed("<p>One</p><p>Two</p>") == Lines{ "One", "Two" });
    // as Sim Settlements 2 leaves a line between two paragraphs
    CHECK(parsed("<p>One</p><br><p>Two</p>") == Lines{ "One", "", "Two" });
    // and as its help has them, with no tag before the first and none after the last
    CHECK(parsed("One. </p><p>Two.") == Lines{ "One. ", "Two." });
}

TEST_CASE("TextMarkup: a paragraph has its align")
{
    CHECK(parsed("<p align='center'>Middle</p>") == Lines{ "center: Middle" });
    CHECK(parsed("<p align=\"right\">End</p>") == Lines{ "right: End" });
    CHECK(parsed("<P ALIGN = \"Center\">Middle</P>") == Lines{ "center: Middle" });
    CHECK(parsed("<p align=center>Middle</p>") == Lines{ "center: Middle" });
    CHECK(parsed("<p align='left'>Start</p>") == Lines{ "Start" });
    CHECK(parsed("<p align='justify'>Start</p>") == Lines{ "Start" });
    // every line of the paragraph, and none after it
    CHECK(parsed("<p align='center'>One<br>Two</p>Three") == Lines{ "center: One", "center: Two", "Three" });
}

TEST_CASE("TextMarkup: a font size is the size of its text")
{
    CHECK(parsed("<font size='24'>A</font>") == Lines{ "[24]A" });
    CHECK(parsed("<font size=\"40\">A</font>") == Lines{ "[40]A" });
    CHECK(parsed("<font size='10'>A</font>") == Lines{ "[10]A" });
    // a size that is not a number above zero is none
    CHECK(parsed("<font size='big'>A</font>") == Lines{ "A" });
    CHECK(parsed("<font size='0'>A</font>") == Lines{ "A" });
    CHECK(parsed("<font size='-3'>A</font>") == Lines{ "A" });
    CHECK(parsed("<font size='30'><font size='big'>A</font></font>") == Lines{ "[30]A" });
}

TEST_CASE("TextMarkup: a font color is the color of its text")
{
    CHECK(parsed("<font color='#FFFFFF'>A</font>") == Lines{ "[#FFFFFF]A" });
    CHECK(parsed("<font color='#0afa78'>A</font>") == Lines{ "[#0AFA78]A" });
    // a color that is not written as #RRGGBB is none
    CHECK(parsed("<font color='red'>A</font>") == Lines{ "A" });
    CHECK(parsed("<font color='#FFF'>A</font>") == Lines{ "A" });
    CHECK(parsed("<font color='#GGGGGG'>A</font>") == Lines{ "A" });
}

TEST_CASE("TextMarkup: the text after a font tag is as the text before it")
{
    const auto lines = model::parseMarkup("Stand <font color='#FFFFFF' size='30'>still</font> now.");

    REQUIRE(lines.size() == 1);
    CHECK(lines[0].runs == std::vector<model::TextRun>{
                               { .text = "Stand ", .size = std::nullopt, .color = std::nullopt },
                               { .text = "still", .size = 30, .color = 0xFFFFFF },
                               { .text = " now.", .size = std::nullopt, .color = std::nullopt },
                           });
}

TEST_CASE("TextMarkup: a font tag inside another keeps what it does not set")
{
    const auto lines = model::parseMarkup("<font size='30'>A<font color='#AAAAAA'>B</font>C</font>D");

    REQUIRE(lines.size() == 1);
    CHECK(lines[0].runs == std::vector<model::TextRun>{
                               { .text = "A", .size = 30, .color = std::nullopt },
                               { .text = "B", .size = 30, .color = 0xAAAAAA },
                               { .text = "C", .size = 30, .color = std::nullopt },
                               { .text = "D", .size = std::nullopt, .color = std::nullopt },
                           });
}

TEST_CASE("TextMarkup: the titles of the test set's mods")
{
    // the font around the paragraph, as FallUI, XDI and Unlimited Survival Mode write it
    CHECK(parsed("<font size='30'><p align='center'>FallUI - Inventory</p></font>") == Lines{ "center: [30]FallUI - Inventory" });
    CHECK(parsed("<font size=\"15\"><p align = \"center\">These settings require restarting game to take effect.</p></font>") ==
          Lines{ "center: [15]These settings require restarting game to take effect." });
    // and inside it, as Sim Settlements 2 and Workshop Framework do
    CHECK(parsed("<p align='center'><font size='24'>Sim Settlements 2</font></p>") == Lines{ "center: [24]Sim Settlements 2" });
    CHECK(parsed("<font color='#AAAAAA' size='16'>One \"EXTRAS>+++\"<br />Two</font>") == Lines{ "[16 #AAAAAA]One \"EXTRAS>+++\"", "[16 #AAAAAA]Two" });
}

TEST_CASE("TextMarkup: any other tag is dropped and its text is kept")
{
    CHECK(parsed("A <b>bold</b>, <i>slanted</i> and <u>underlined</u> word") == Lines{ "A bold, slanted and underlined word" });
    CHECK(parsed("<a href='https://example.com'>A link</a><img src='x'/>") == Lines{ "A link" });
    CHECK(parsed("</font>A closing tag with none open") == Lines{ "A closing tag with none open" });
}

TEST_CASE("TextMarkup: what is not a tag is text")
{
    CHECK(parsed("Less < more") == Lines{ "Less < more" });
    CHECK(parsed("1 < 2 and 3 > 2") == Lines{ "1 < 2 and 3 > 2" });
    CHECK(parsed("An open <font") == Lines{ "An open <font" });
    CHECK(parsed("<>") == Lines{ "<>" });
}

TEST_CASE("TextMarkup: entities are decoded")
{
    CHECK(parsed("Salt &amp; pepper &lt;b&gt; &quot;said&quot; it&apos;s&nbsp;so") == Lines{ "Salt & pepper <b> \"said\" it's so" });
    CHECK(parsed("&#65;&#x42;&#xe9;&#8364;") == Lines{ "AB\xC3\xA9\xE2\x82\xAC" });
    // what is not one stays as it is written
    CHECK(parsed("Fish & chips; &unknown; &#; &#x110000; &amp") == Lines{ "Fish & chips; &unknown; &#; &#x110000; &amp" });
}

TEST_CASE("TextMarkup: the empty lines at the end of a text are left out")
{
    CHECK(parsed("One<br><br>") == Lines{ "One" });
    CHECK(parsed("One\n") == Lines{ "One" });
    CHECK(parsed("<br>One") == Lines{ "", "One" });
    CHECK(parsed("<p></p><br>").empty());
}

TEST_CASE("TextMarkup: a text that takes no tags is shown as it is written")
{
    CHECK(describe(model::plainLines("Customize the item cards.\nThis includes: Pipboy.")) == Lines{ "Customize the item cards.", "This includes: Pipboy." });
    CHECK(describe(model::plainLines("<b>Salt</b> &amp; pepper<br>")) == Lines{ "<b>Salt</b> &amp; pepper<br>" });
}

TEST_CASE("TextMarkup: a text without its tags is on one line")
{
    CHECK(model::removeMarkup("Turns the feature on.") == "Turns the feature on.");
    CHECK(model::removeMarkup("Icons scale in percent. <font color='#aaaaaa'>(Default: 100%)</font>") == "Icons scale in percent. (Default: 100%)");
    CHECK(model::removeMarkup("<p>When ON, settlers assign themselves.</p>") == "When ON, settlers assign themselves.");
    // a line break and the end of a paragraph become one space
    CHECK(model::removeMarkup("Restock every 48 hours. </p><p>When OFF, by the economy.") == "Restock every 48 hours. When OFF, by the economy.");
    CHECK(model::removeMarkup("One<br><br>Two\nThree") == "One Two Three");
    CHECK(model::removeMarkup("Salt &amp; pepper") == "Salt & pepper");
    CHECK(model::removeMarkup("<p> </p><br>").empty());
}
