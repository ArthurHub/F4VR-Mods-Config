#include <catch2/catch_test_macros.hpp>

#include "TestFiles.h"
#include "mcm/FallUITranslations.h"
#include "mcm/McmLoader.h"

using namespace mods_config;

namespace
{
    // lines of a language file, as FallUI ships them
    constexpr auto ENGLISH_FILE =
        "$_TRANSLATION_THIS_LANGUAGE_NAME\tEnglish\r\n"
        "$FallUI_Inventory\tFallUI - Inventory\r\n"
        "$General_settings\tGeneral settings\r\n"
        "$Key\tKey\r\n"
        "$STAT\tSTAT\r\n"
        "$Perks\tPerks\r\n"
        "$Show_button_hint_bar\tShow button hint bar\r\n"
        "$helpShowButtonBar\tWill display the bottom bar.\r\n"
        "$Max_item_name_length\tMax. item name length\r\n"
        "$Item_cards_Use_colored_difference_text\tItem cards - Use colored difference text\r\n"
        "$ContainerBarterCompanion\tContainer/Barter/Companion\r\n"
        "$Default_100\tDefault: 100%\r\n"
        "$descNote\tThe item is in your HUD color.\r\n";

    constexpr auto GERMAN_FILE =
        "$_TRANSLATION_THIS_LANGUAGE_NAME\tDeutsch\r\n"
        "$FallUI_Inventory\tFallUI - Inventar\r\n"
        "$General_settings\tAllgemeine Einstellungen\r\n"
        "$Key\tTaste\r\n"
        "$Perks\tExtras\r\n"
        "$Show_button_hint_bar\tTastenleiste zeigen\r\n"
        "$helpShowButtonBar\tZeigt die untere Leiste.\r\n"
        "$Max_item_name_length\tMax. Länge des Namens\r\n"
        "$Item_cards_Use_colored_difference_text\tKarten - farbiger Text\r\n"
        "$ContainerBarterCompanion\tBehälter/Handel/Begleiter\r\n"
        "$Default_100\tStandard: 100%\r\n"
        "$descNote\tDer Gegenstand hat die Farbe des HUD.\r\n";

    /**
     * The English texts, and over them the German ones.
     */
    mcm::FallUITranslations german(const model::Translations& game)
    {
        mcm::FallUITranslations fallUI(game);
        fallUI.addFile(ENGLISH_FILE);
        fallUI.addFile(GERMAN_FILE);
        return fallUI;
    }
}

TEST_CASE("FallUITranslations: a text that is a token becomes the text of FallUI's file")
{
    const model::Translations game;
    const auto fallUI = german(game);

    CHECK(fallUI.translate("$General_settings") == "Allgemeine Einstellungen");
    // a token that only the English file has
    CHECK(fallUI.translate("$STAT") == "STAT");
    CHECK(fallUI.translate("$Missing") == "$Missing");
    CHECK(fallUI.translate("").empty());
    CHECK(fallUI.translate("   ") == "   ");
}

TEST_CASE("FallUITranslations: a token inside a text is replaced")
{
    model::Translations game;
    game.add("$Attach", "ATTACH");
    game.add("$TAB", "TAB");
    const auto fallUI = german(game);

    CHECK(fallUI.translate("    - ${Key}") == "    - Taste");
    CHECK(fallUI.translate("${STAT} > ${Perks}") == "STAT > Extras");
    CHECK(fallUI.translate("${Key} Q") == "Taste Q");
    CHECK(fallUI.translate("<font color='#AAAAAA' size='16'>${descNote}</font>") == "<font color='#AAAAAA' size='16'>Der Gegenstand hat die Farbe des HUD.</font>");
    // a token that FallUI's files lack is the game's
    CHECK(fallUI.translate("${Key} ${TAB}") == "Taste TAB");
    // and one that no file has is left as a token
    CHECK(fallUI.translate("${Key} ${Missing}") == "Taste $Missing");
    // what is not a token stays
    CHECK(fallUI.translate("${} and ${Key") == "${} and ${Key");
}

TEST_CASE("FallUITranslations: a plain text is looked up by the token of its words")
{
    const model::Translations game;
    const auto fallUI = german(game);

    CHECK(fallUI.translate("Show button hint bar") == "Tastenleiste zeigen");
    CHECK(fallUI.translate("Max. item name length") == "Max. Länge des Namens");
    CHECK(fallUI.translate("Item cards - Use colored difference text") == "Karten - farbiger Text");
    CHECK(fallUI.translate("Default: 100%") == "Standard: 100%");
    CHECK(fallUI.translate("No such text") == "No such text");
    // the spaces that set a page in under another stay
    CHECK(fallUI.translate("    Container/Barter/Companion") == "    Behälter/Handel/Begleiter");
    // and so does what is around the words
    CHECK(fallUI.translate(" - Key...") == " - Taste...");
    CHECK(fallUI.translate("(Key)") == "(Taste)");
    CHECK(fallUI.translate("Key: 12") == "Taste: 12");
}

TEST_CASE("FallUITranslations: each piece of a text between its tags is translated by itself")
{
    const model::Translations game;
    const auto fallUI = german(game);

    CHECK(fallUI.translate("<font size='30'><p align='center'>FallUI - Inventory</p></font>") == "<font size='30'><p align='center'>FallUI - Inventar</p></font>");
    CHECK(fallUI.translate("<b>Key</b> for <i>Perks</i>") == "<b>Taste</b> for <i>Extras</i>");
    CHECK(fallUI.translate("Key\nPerks") == "Taste\nExtras");
}

TEST_CASE("FallUITranslations: a text with a key of its own is looked up by it")
{
    const model::Translations game;
    const auto fallUI = german(game);

    CHECK(fallUI.translate("Will display the bottom bar with available actions and keys.", "$helpShowButtonBar") == "Zeigt die untere Leiste.");
    // a key that no file has leaves the text as it is written, and its words are not looked up
    CHECK(fallUI.translate("Key", "$helpMissing") == "Key");
}

TEST_CASE("FallUITranslations: a line of a file is a token, a tab and its text")
{
    const model::Translations game;
    mcm::FallUITranslations fallUI(game);
    fallUI.addFile("$One\tFirst\n$NoText\n$NoTextAfterTab\t\n\n$Three\tfields\there\r\n$Two\tSecond");

    CHECK(fallUI.translate("$One") == "First");
    CHECK(fallUI.translate("$Two") == "Second");
    CHECK(fallUI.translate("$NoText") == "$NoText");
    CHECK(fallUI.translate("$NoTextAfterTab") == "$NoTextAfterTab");
    CHECK(fallUI.translate("$Three") == "$Three");

    // a later text replaces the one of its token
    fallUI.addFile("$One\tErste");
    CHECK(fallUI.translate("$One") == "Erste");
}

TEST_CASE("FallUITranslations: the file of the game's language is read over the English one")
{
    const test_files::TempFolder folder;
    folder.write("Interface/FallUI/Translation/MCMConfig_en.txt", ENGLISH_FILE);
    folder.write("Interface/FallUI/Translation/MCMConfig_de.txt", GERMAN_FILE);
    const auto languageFile = "FallUI/Translation/MCMConfig_${LANGUAGE_CODE}.txt";

    // FallUI tells the game's language by its text of `$Attach`
    model::Translations germanGame;
    germanGame.add("$Attach", "ANBRINGEN");
    mcm::FallUITranslations inGerman(germanGame);
    inGerman.loadFiles(languageFile, folder.path / "Interface");
    CHECK(inGerman.translate("    - ${Key}") == "    - Taste");
    CHECK(inGerman.translate("$STAT") == "STAT");

    model::Translations englishGame;
    englishGame.add("$Attach", "ATTACH");
    mcm::FallUITranslations inEnglish(englishGame);
    inEnglish.loadFiles(languageFile, folder.path / "Interface");
    CHECK(inEnglish.translate("    - ${Key}") == "    - Key");

    // a language FallUI has no file for is shown in English, and so is a game it cannot tell the language of
    model::Translations frenchGame;
    frenchGame.add("$Attach", "FIXER");
    mcm::FallUITranslations inFrench(frenchGame);
    inFrench.loadFiles(languageFile, folder.path / "Interface");
    CHECK(inFrench.translate("    - ${Key}") == "    - Key");

    const model::Translations unknownGame;
    mcm::FallUITranslations inUnknown(unknownGame);
    inUnknown.loadFiles(languageFile, folder.path / "Interface");
    CHECK(inUnknown.translate("    - ${Key}") == "    - Key");

    // with no file every text is as it is written
    mcm::FallUITranslations noFiles(germanGame);
    noFiles.loadFiles(languageFile, folder.path / "Missing");
    CHECK(noFiles.translate("Show button hint bar") == "Show button hint bar");
    CHECK(noFiles.translate("    - ${Key}") == "    - $Key");
}

TEST_CASE("McmLoader: a FallUI mod is translated from the language files its config names")
{
    const test_files::TempFolder folder;
    // FallUI Workbench keeps its files beside its config, and names them from the interface folder
    folder.write("MCM/Config/FallUI/Translation/MCM_en.txt", ENGLISH_FILE);
    folder.write("MCM/Config/FallUI/Translation/MCM_de.txt", GERMAN_FILE);
    folder.write("MCM/Config/FallUI/config.json", R"({
        "modName": "FallUI",
        "displayName": "FallUI - Inventory",
        "content": [
            { "type": "text", "text": "<p align='center'>FallUI - Inventory</p>", "html": true },
            { "type": "image", "$fallUILanguageFile": "../MCM/Config/FallUI/Translation/MCM_${LANGUAGE_CODE}.txt" }
        ],
        "pages": [
            {
                "pageDisplayName": "$General_settings",
                "content": [
                    { "type": "section", "text": "Show button hint bar" },
                    { "type": "textinput", "text": "    - ${Key}" },
                    { "type": "switcher", "text": "$Attach" }
                ]
            },
            { "pageDisplayName": "    Container/Barter/Companion", "content": [] }
        ]
    })");
    folder.write("MCM/Config/Other/config.json", R"({ "modName": "Other", "displayName": "General settings", "content": [{ "type": "button", "text": "${Key}" }] })");
    model::Translations game;
    game.add("$Attach", "ANBRINGEN");

    const auto mods = mcm::loadConfigs(folder.path / "MCM" / "Config", game, folder.path / "Interface");

    REQUIRE(mods.size() == 2);
    const auto& fallUI = mods[0].name == "FallUI" ? mods[0] : mods[1];
    const auto& other = mods[0].name == "FallUI" ? mods[1] : mods[0];

    CHECK(fallUI.displayName == "FallUI - Inventar");
    REQUIRE(fallUI.frontPage.items.size() == 2);
    CHECK(std::get<model::Text>(fallUI.frontPage.items[0]).text == "<p align='center'>FallUI - Inventar</p>");
    REQUIRE(fallUI.pages.size() == 2);
    CHECK(fallUI.pages[0].name == "Allgemeine Einstellungen");
    CHECK(fallUI.pages[1].name == "    Behälter/Handel/Begleiter");
    REQUIRE(fallUI.pages[0].items.size() == 1);
    const auto& group = std::get<model::Group>(fallUI.pages[0].items[0]);
    CHECK(group.text == "Tastenleiste zeigen");
    REQUIRE(group.items.size() == 2);
    CHECK(std::get<model::Setting>(group.items[0]).text == "    - Taste");
    // a token that FallUI's files lack is the game's
    CHECK(std::get<model::Setting>(group.items[1]).text == "ANBRINGEN");

    // a mod that names no language file is not translated from FallUI's
    CHECK(other.displayName == "General settings");
    REQUIRE(other.frontPage.items.size() == 1);
    CHECK(std::get<model::Button>(other.frontPage.items[0]).text == "${Key}");
}
