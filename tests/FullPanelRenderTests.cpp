#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "TestGlobals.h"
#include "TestPanel.h"
#include "Theme.h"
#include "mcm/McmLoader.h"
#include "mcm/McmStore.h"
#include "model/TextMarkup.h"
#include "ui/FullPanel.h"

// The Full panel drawn without the game and saved as an image, to look at a change of the UI before it is tried
// in the headset. They check nothing, so they are not run with the other tests. Run them by their tag:
//     ModsConfigVR_tests "[render]"
// Each says where its image is, a .bmp file in the build folder.

using namespace mods_config;
using test_panel::POINTER_AWAY;
using ui::FullPanel;
using ui::Selection;

namespace
{
    // the world behind the panel's glass
    constexpr std::array<std::uint8_t, 3> WORLD{ 58, 60, 58 };

    struct Image
    {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> rgb;

        Image(const int width, const int height)
            : width(width),
              height(height),
              rgb(static_cast<std::size_t>(width) * height * 3)
        {
            for (std::size_t i = 0; i < rgb.size(); ++i) {
                rgb[i] = WORLD[i % 3];
            }
        }

        void blend(const int x, const int y, const std::array<float, 3>& color, const float alpha)
        {
            auto* pixel = &rgb[(static_cast<std::size_t>(y) * width + x) * 3];
            for (int i = 0; i < 3; ++i) {
                pixel[i] = static_cast<std::uint8_t>(std::lround(color[i] * alpha + pixel[i] * (1.0f - alpha)));
            }
        }
    };

    /**
     * ImGui's font texture, one byte of opacity for a pixel, read between its pixels.
     */
    struct FontTexture
    {
        unsigned char* alpha = nullptr;
        int width = 0;
        int height = 0;

        FontTexture()
        {
            ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&alpha, &width, &height);
        }

        float at(const int x, const int y) const
        {
            return alpha[std::clamp(y, 0, height - 1) * width + std::clamp(x, 0, width - 1)] / 255.0f;
        }

        float sample(const float u, const float v) const
        {
            const float x = u * width - 0.5f;
            const float y = v * height - 0.5f;
            const int left = static_cast<int>(std::floor(x));
            const int top = static_cast<int>(std::floor(y));
            const float fx = x - left;
            const float fy = y - top;
            return std::lerp(std::lerp(at(left, top), at(left + 1, top), fx), std::lerp(at(left, top + 1), at(left + 1, top + 1), fx), fy);
        }
    };

    // A pixel is split in this many steps for a triangle's corners. The steps are whole numbers, so two
    // triangles that share an edge agree on every pixel along it, and a see-through fill has no seam.
    constexpr std::int64_t SUBPIXEL = 256;

    struct Corner
    {
        std::int64_t x = 0;
        std::int64_t y = 0;
        std::array<float, 4> color{};
        ImVec2 uv;

        Corner(const ImDrawVert& vertex, const float scale)
            : x(std::llround(vertex.pos.x * scale * SUBPIXEL)),
              y(std::llround(vertex.pos.y * scale * SUBPIXEL)),
              color{ static_cast<float>(vertex.col & 0xFF),
                  static_cast<float>((vertex.col >> 8) & 0xFF),
                  static_cast<float>((vertex.col >> 16) & 0xFF),
                  static_cast<float>(vertex.col >> 24) },
              uv(vertex.uv)
        {}
    };

    /**
     * Which side of the edge from a to b a point is on, and whether a point that is on the edge itself is in
     * the triangle. Of two triangles that share an edge, one takes its points.
     */
    bool isInside(const Corner& a, const Corner& b, const std::int64_t x, const std::int64_t y, std::int64_t& side)
    {
        const std::int64_t dx = b.x - a.x;
        const std::int64_t dy = b.y - a.y;
        side = dx * (y - a.y) - dy * (x - a.x);
        return side > 0 || (side == 0 && (dy > 0 || (dy == 0 && dx < 0)));
    }

    /**
     * A pixel of an image of a mod's page, read at the nearest one: its color, and its opacity.
     */
    std::array<float, 4> sample(const model::ImagePixels& pixels, const float u, const float v)
    {
        const auto x = static_cast<std::size_t>(std::clamp(static_cast<int>(u * pixels.width), 0, static_cast<int>(pixels.width) - 1));
        const auto y = static_cast<std::size_t>(std::clamp(static_cast<int>(v * pixels.height), 0, static_cast<int>(pixels.height) - 1));
        const auto* pixel = &pixels.rgba[(y * pixels.width + x) * 4];
        return { static_cast<float>(pixel[0]), static_cast<float>(pixel[1]), static_cast<float>(pixel[2]), pixel[3] / 255.0f };
    }

    /**
     * Fills one triangle of ImGui's, with the colors of its corners and the opacity of the font texture.
     * A triangle of an image of a mod's page has the image's colors and opacity in place of them.
     */
    void fillTriangle(Image& image, const FontTexture& font, const model::ImagePixels* pixels, Corner a, Corner b, Corner c, const ImVec4& clip)
    {
        const std::int64_t area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (area == 0) {
            return;
        }
        if (area < 0) {
            std::swap(b, c);
        }
        const auto toPixel = [](const std::int64_t value) {
            return static_cast<int>(value / SUBPIXEL);
        };
        const int left = (std::max)(toPixel((std::min)({ a.x, b.x, c.x })), static_cast<int>(std::floor(clip.x)));
        const int top = (std::max)(toPixel((std::min)({ a.y, b.y, c.y })), static_cast<int>(std::floor(clip.y)));
        const int right = (std::min)(toPixel((std::max)({ a.x, b.x, c.x })) + 1, static_cast<int>(std::ceil(clip.z)));
        const int bottom = (std::min)(toPixel((std::max)({ a.y, b.y, c.y })) + 1, static_cast<int>(std::ceil(clip.w)));

        for (int y = (std::max)(top, 0); y < (std::min)(bottom, image.height); ++y) {
            for (int x = (std::max)(left, 0); x < (std::min)(right, image.width); ++x) {
                const std::int64_t px = x * SUBPIXEL + SUBPIXEL / 2;
                const std::int64_t py = y * SUBPIXEL + SUBPIXEL / 2;
                std::int64_t sideA = 0;
                std::int64_t sideB = 0;
                std::int64_t sideC = 0;
                if (!isInside(b, c, px, py, sideA) || !isInside(c, a, px, py, sideB) || !isInside(a, b, px, py, sideC)) {
                    continue;
                }
                const float total = static_cast<float>(sideA + sideB + sideC);
                const float weightA = sideA / total;
                const float weightB = sideB / total;
                const float weightC = sideC / total;
                const auto mix = [&](const float ofA, const float ofB, const float ofC) {
                    return ofA * weightA + ofB * weightB + ofC * weightC;
                };
                const float u = mix(a.uv.x, b.uv.x, c.uv.x);
                const float v = mix(a.uv.y, b.uv.y, c.uv.y);
                if (pixels) {
                    const auto color = sample(*pixels, u, v);
                    image.blend(x, y, { color[0], color[1], color[2] }, color[3]);
                    continue;
                }
                const float opacity = mix(a.color[3], b.color[3], c.color[3]) / 255.0f * font.sample(u, v);
                image.blend(x, y, { mix(a.color[0], b.color[0], c.color[0]), mix(a.color[1], b.color[1], c.color[1]), mix(a.color[2], b.color[2], c.color[2]) }, opacity);
            }
        }
    }

    /**
     * Draws a frame of ImGui's into an image, as a renderer does: every triangle of every draw command,
     * inside the command's clip rectangle. A command with a texture that is not the font's is an image of a
     * mod's page, whose texture is its pixels.
     */
    Image rasterize(const ImDrawData& drawData, const FontTexture& font, const float scale)
    {
        Image image(static_cast<int>(std::lround(drawData.DisplaySize.x * scale)), static_cast<int>(std::lround(drawData.DisplaySize.y * scale)));
        for (const auto* commandList : drawData.CmdLists) {
            for (const auto& command : commandList->CmdBuffer) {
                if (command.UserCallback) {
                    continue;
                }
                const ImVec4 clip(command.ClipRect.x * scale, command.ClipRect.y * scale, command.ClipRect.z * scale, command.ClipRect.w * scale);
                const auto* indexes = commandList->IdxBuffer.Data + command.IdxOffset;
                const auto* vertexes = commandList->VtxBuffer.Data + command.VtxOffset;
                const auto* pixels = command.TextureId == ImGui::GetIO().Fonts->TexID ? nullptr : static_cast<const model::ImagePixels*>(command.TextureId);
                for (unsigned int i = 0; i + 2 < command.ElemCount; i += 3) {
                    fillTriangle(image, font, pixels, Corner(vertexes[indexes[i]], scale), Corner(vertexes[indexes[i + 1]], scale), Corner(vertexes[indexes[i + 2]], scale), clip);
                }
            }
        }
        return image;
    }

    /**
     * Saves an image as a .bmp file: 24 bits a pixel, with its rows from the bottom one up.
     */
    void saveBmp(const std::filesystem::path& file, const Image& image)
    {
        const int rowSize = (image.width * 3 + 3) & ~3;
        const auto dataSize = static_cast<std::uint32_t>(rowSize * image.height);
        std::array<std::uint8_t, 54> header{};
        const auto put = [&header](const std::size_t at, const std::uint32_t value) {
            for (std::size_t i = 0; i < 4; ++i) {
                header[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
            }
        };
        header[0] = 'B';
        header[1] = 'M';
        put(2, 54 + dataSize);
        put(10, 54);
        put(14, 40);
        put(18, static_cast<std::uint32_t>(image.width));
        put(22, static_cast<std::uint32_t>(image.height));
        header[26] = 1;
        header[28] = 24;
        put(34, dataSize);

        std::vector<std::uint8_t> row(rowSize);
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(header.data()), header.size());
        for (int y = image.height - 1; y >= 0; --y) {
            for (int x = 0; x < image.width; ++x) {
                const auto* pixel = &image.rgb[(static_cast<std::size_t>(y) * image.width + x) * 3];
                row[x * 3] = pixel[2];
                row[x * 3 + 1] = pixel[1];
                row[x * 3 + 2] = pixel[0];
            }
            stream.write(reinterpret_cast<const char*>(row.data()), row.size());
        }
    }

    /**
     * Draws the panel with the pointer at a place and saves it as an image, at the size the framework
     * rasterizes the panel. A few frames are drawn first: ImGui needs them to find what the pointer is on, and
     * to scroll to a row. What is given to run before them draws the frames that bring the panel to the
     * state it is drawn in, as a press that opens a list.
     */
    std::string renderToFile(FullPanel& panel, const std::string& name, const ImVec2 pointer, const bool pressed = false, const std::function<void()>& before = {})
    {
        const test_panel::PanelContext context;
        // an image is drawn with its pixels as its texture, which the rasterizer reads
        panel.setImageDrawer([](const model::ImagePixels& pixels, const ImVec2& min, const ImVec2& max) {
            ImGui::GetWindowDrawList()->AddImage(const_cast<model::ImagePixels*>(&pixels), min, max);
        });
        if (before) {
            before();
        }
        // the ray comes there, and then the trigger is pulled and held
        for (int i = 0; i < 5; ++i) {
            test_panel::drawFrame(panel, pointer, pressed && i >= 3);
        }
        const std::filesystem::path folder = MODS_CONFIG_TEST_OUTPUT_DIR;
        std::filesystem::create_directories(folder);
        const auto file = folder / (name + ".bmp");
        saveBmp(file, rasterize(*ImGui::GetDrawData(), FontTexture(), test_panel::SUPERSAMPLE));
        return file.string();
    }

    model::Item setting(const model::SettingType type, std::string text, model::ValueOptions valueOptions = {})
    {
        return model::Setting{ .type = type, .text = std::move(text), .valueOptions = std::move(valueOptions) };
    }

    /**
     * The options of a choice, each with its place in the list as its value.
     */
    model::ValueOptions optionsOf(const std::vector<std::string>& texts)
    {
        model::ValueOptions valueOptions;
        for (const auto& text : texts) {
            valueOptions.options.push_back({ .value = static_cast<int>(valueOptions.options.size()), .text = text });
        }
        return valueOptions;
    }

    /**
     * A page with a row of every kind a mod's menu has, some of them in sections.
     */
    model::Page pageOfEveryRow(std::string name)
    {
        using model::SettingType;

        model::Group general{ .text = "General", .items = {} };
        general.items.push_back(setting(SettingType::Switcher, "Enable the feature"));
        general.items.push_back(setting(SettingType::Slider, "Strength", { .min = 0.0f, .max = 2.0f, .step = 0.1f, .options = {} }));
        general.items.push_back(setting(SettingType::Stepper, "Hours per battery", optionsOf({ "2", "4", "8" })));
        general.items.push_back(setting(SettingType::Dropdown, "Mode", optionsOf({ "Spot", "Flood", "Lantern" })));
        general.items.push_back(model::Spacer{ .lines = 2 });
        general.items.push_back(setting(SettingType::Switcher, "A name that is too long for the room a row has before what the row is"));

        model::Group tools{ .text = "Tools", .items = {} };
        tools.items.push_back(setting(SettingType::TextInput, "Name of the settlement"));
        tools.items.push_back(model::Placeholder{ .text = "Open the menu", .type = "hotkey" });
        tools.items.push_back(model::Button{ .text = "Reset everything" });
        tools.items.push_back(model::Placeholder{ .text = "A row with no type", .type = "" });

        model::Page page{ .name = std::move(name), .items = {} };
        page.items.push_back(model::Text{ .lines = model::parseMarkup("<p align='center'><font size='24'>The mod's title</font></p>"
                                                                      "<p>What the mod is, in a text row at the top of its page. It is long enough to be wrapped to the "
                                                                      "width of the rows, <font color='#FFFFFF'>with some words in a color of their own</font>.</p><br>"
                                                                      "<p align='right'><font size='16'>A note in the detail size, at the right</font></p>") });
        page.items.push_back(model::Image{});
        page.items.push_back(model::Text{ .lines = model::plainLines("A text row with no tags.\nIts second line.") });
        page.items.push_back(std::move(general));
        page.items.push_back(std::move(tools));
        return page;
    }

    /**
     * A page with more rows than the pane has room for.
     */
    model::Page longPage(std::string name)
    {
        model::Page page{ .name = std::move(name), .items = {} };
        for (int i = 1; i <= 24; ++i) {
            page.items.push_back(setting(model::SettingType::Switcher, std::format("Option {}", i)));
        }
        return page;
    }

    model::Mod modOf(const std::string& name, const std::vector<std::string>& pages = {})
    {
        model::Mod mod{ .name = name, .displayName = name, .frontPage = pageOfEveryRow({}), .pages = {} };
        for (const auto& page : pages) {
            mod.pages.push_back(longPage(page));
        }
        return mod;
    }

    /**
     * The panel with the mods of the test set, each with the pages its config has, and FRIK in its quick
     * launch section.
     */
    FullPanel testSetPanel()
    {
        std::vector<model::Mod> mods;
        mods.push_back(modOf("XDI"));
        mods.push_back(modOf("Sim Settlements 2", { "Hotkeys", "Gameplay", "HUD", "Notifications", "Performance", "Tools" }));
        mods.push_back(modOf("Auto Eat and Drink in Survival"));
        mods.push_back(modOf("FallUI - Workbench", { "$General_settings", "$Interface_coloring", "$Workbench_list" }));
        mods.push_back(modOf("Chemfluence"));
        mods.push_back(modOf("NPCs Use Items", { "Factions" }));
        mods.push_back(
            modOf("Workshop Framework", { "Hotkeys", "Settlement Gameplay", "Door Management Options", "Framework Controls", "Message Options", "Tools", "Advanced Controls" }));
        // FallUI sets a page in under another by spaces before its name
        mods.push_back(modOf("FallUI - Inventory",
            { "Interface", "    Coloring", "    Text style", "Lists", "    Pipboy", "    Container/Barter/Companion", "    Columns", "Item cards", "Expert options" }));
        mods.push_back(modOf("Unlimited Survival Mode"));

        FullPanel panel;
        panel.setMods(std::move(mods));
        panel.setQuickLaunch({ { .name = "FRIK", .help = "Opens FRIK's own configuration in the world: body height, arm length, and the weapon and Pip-Boy positions.", .open = [] {
                                } } });
        return panel;
    }

    /**
     * A panel on a page whose rows have a help of each length: one line, two lines, three lines in the smaller
     * size, more than the footer has room for, and none under a name that is too long for its row.
     */
    FullPanel helpPanel()
    {
        const std::string sentence = "Hides the compass, health, action points and the ammo counter while the Pip-Boy is raised, and brings them back when it is lowered. ";
        const auto row = [](std::string text, std::string help) {
            return model::Setting{ .type = model::SettingType::Switcher, .text = std::move(text), .help = std::move(help), .valueOptions = {} };
        };
        model::Mod mod{ .name = "Help", .displayName = "The help line", .frontPage = {}, .pages = {} };
        mod.frontPage.items.push_back(row("Brightness", "How bright the beam is. Default 1.00."));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + "Default off."));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + sentence + "Default off."));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + sentence + sentence + sentence + "Default off."));
        mod.frontPage.items.push_back(row("A switcher with a name that is much too long for the room a row has before its control and ends in an ellipsis", {}));

        FullPanel panel;
        panel.setMods({ std::move(mod) });
        panel.select({ .mod = 0, .page = std::nullopt });
        return panel;
    }

    /**
     * The middle of a row of a page that has rows of controls alone, by its place among them.
     */
    ImVec2 pageRow(const int place)
    {
        const float rowsTop = test_panel::CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3;
        return { 700.0f, rowsTop + static_cast<float>(place) * theme::ROW_PITCH + theme::CONTROL_HEIGHT * 0.5f };
    }

    /**
     * Shows a mod of the panel by its name, and one of its pages by the page's.
     */
    void select(FullPanel& panel, const std::string_view mod, const std::string_view page = {})
    {
        const auto& mods = panel.mods();
        const auto found = std::ranges::find(mods, mod, &model::Mod::displayName);
        REQUIRE(found != mods.end());
        Selection selection{ .mod = static_cast<std::size_t>(found - mods.begin()), .page = std::nullopt };
        if (!page.empty()) {
            const auto foundPage = std::ranges::find(found->pages, page, &model::Page::name);
            REQUIRE(foundPage != found->pages.end());
            selection.page = static_cast<std::size_t>(foundPage - found->pages.begin());
        }
        panel.select(selection);
    }
}

TEST_CASE("FullPanel render: the menu's front page", "[.render]")
{
    auto panel = testSetPanel();
    WARN("Saved " << renderToFile(panel, "front-page", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the ray on a quick launch row", "[.render]")
{
    auto panel = testSetPanel();
    WARN("Saved " << renderToFile(panel, "front-page-quick-launch-hover", ImVec2(640.0f, 250.0f)));
}

TEST_CASE("FullPanel render: no mods and no quick launch", "[.render]")
{
    FullPanel panel;
    WARN("Saved " << renderToFile(panel, "front-page-empty", POINTER_AWAY));
}

TEST_CASE("FullPanel render: a mod's front page, with the ray on the row of another mod", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Sim Settlements 2");
    WARN("Saved " << renderToFile(panel, "mod-front-page", ImVec2(150.0f, 275.0f)));
}

TEST_CASE("FullPanel render: a mod with no pages keeps its arrow when it is selected", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Chemfluence");
    WARN("Saved " << renderToFile(panel, "mod-no-pages", POINTER_AWAY));
}

TEST_CASE("FullPanel render: a page of a mod, with the ray on the mod's breadcrumb", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Sim Settlements 2", "Gameplay");
    WARN("Saved " << renderToFile(panel, "mod-page", ImVec2(360.0f, 50.0f)));
}

TEST_CASE("FullPanel render: a page under the tree's last rows is scrolled to", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Workshop Framework", "Advanced Controls");
    WARN("Saved " << renderToFile(panel, "mod-page-scrolled-to", POINTER_AWAY));
}

TEST_CASE("FullPanel render: pages that are set in under another by spaces", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "FallUI - Inventory", "    Container/Barter/Companion");
    WARN("Saved " << renderToFile(panel, "mod-page-set-in", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the pages of the MCM showcase", "[.render]")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);

    // the showcase has no game plugin, so none of its globals is in the game
    test_globals::TestGlobals globals;
    mcm::McmStore store(globals);

    // a panel for each image: a panel sets its style once, on the ImGui context it is first drawn in
    const auto render = [&mods, &store](const std::optional<std::size_t> page, const std::string& name) {
        FullPanel panel;
        panel.setStore(&store);
        panel.setMods(mods);
        panel.select({ .mod = 0, .page = page });
        WARN("Saved " << renderToFile(panel, name, POINTER_AWAY));
    };
    render(std::nullopt, "mcm-showcase-front-page");
    for (std::size_t page = 0; page < mods[0].pages.size(); ++page) {
        render(page, std::format("mcm-showcase-page-{}", page + 1));
    }
}

TEST_CASE("FullPanel render: the values of a page's settings, and the status of the mod", "[.render]")
{
    using model::SettingType;
    const auto global = [](const SettingType type, std::string text, const std::uint32_t formId, model::ValueOptions valueOptions = {}) {
        return model::Setting{ .type = type,
            .text = std::move(text),
            .help = "What the setting does.",
            .valueOptions = std::move(valueOptions),
            .storage = model::GlobalStorage{ .plugin = "MyMod.esp", .formId = formId } };
    };
    test_globals::TestGlobals globals;
    globals.values = { { { "MyMod.esp", 0x800 }, 1.0f }, { { "MyMod.esp", 0x801 }, 0.0f }, { { "MyMod.esp", 0x802 }, 1.3f }, { { "MyMod.esp", 0x803 }, 2.0f } };
    mcm::McmStore store(globals);

    model::Mod mod{ .name = "Values", .displayName = "Values", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(global(SettingType::Switcher, "A switcher that is on", 0x800));
    mod.frontPage.items.push_back(global(SettingType::Switcher, "A switcher that is off", 0x801));
    mod.frontPage.items.push_back(global(SettingType::Slider, "A slider at 1.3", 0x802, { .min = 0.0f, .max = 2.0f, .step = 0.1f, .options = {} }));
    mod.frontPage.items.push_back(global(SettingType::Stepper, "A stepper at its third option", 0x803, optionsOf({ "2", "4", "8" })));
    mod.frontPage.items.push_back(global(SettingType::Dropdown, "A dropdown at its third option", 0x803, optionsOf({ "Spot", "Flood", "Lantern" })));
    mod.frontPage.items.push_back(global(SettingType::Switcher, "A global that is not in the game", 0x1234));
    mod.frontPage.items.push_back(setting(SettingType::Switcher, "A switcher that is kept nowhere"));

    // with the ray off the rows, where the status is, and on the row whose value is not available
    for (const bool onRow : { false, true }) {
        FullPanel panel;
        panel.setStore(&store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = std::nullopt });
        WARN("Saved " << renderToFile(panel, onRow ? "values-not-available" : "values", onRow ? pageRow(5) : POINTER_AWAY));
    }
}

TEST_CASE("FullPanel render: a slider that is dragged", "[.render]")
{
    using model::SettingType;
    const auto slider = [](std::string text, const std::uint32_t formId, const float max, const float step) {
        return model::Setting{ .type = SettingType::Slider,
            .text = std::move(text),
            .help = "How strong the feature is.",
            .valueOptions = { .min = 0.0f, .max = max, .step = step, .options = {} },
            .storage = model::GlobalStorage{ .plugin = "MyMod.esp", .formId = formId } };
    };
    test_globals::TestGlobals globals;
    globals.values = { { { "MyMod.esp", 0x800 }, 0.5f }, { { "MyMod.esp", 0x801 }, 40.0f } };
    mcm::McmStore store(globals);

    model::Mod mod{ .name = "Sliders", .displayName = "Sliders", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(slider("Strength", 0x800, 2.0f, 0.1f));
    mod.frontPage.items.push_back(slider("Brightness", 0x801, 100.0f, 5.0f));
    mod.frontPage.items.push_back(slider("With no range", 0x801, 0.0f, 1.0f));

    FullPanel panel;
    panel.setStore(&store);
    panel.setMods({ std::move(mod) });
    panel.select({ .mod = 0, .page = std::nullopt });
    // on the track of the first slider, with the trigger held
    WARN("Saved " << renderToFile(panel, "slider-dragged", ImVec2(820.0f, pageRow(0).y), true));
}

TEST_CASE("FullPanel render: a stepper under the ray, and the open list of a dropdown", "[.render]")
{
    using model::SettingType;
    const auto choice = [](const SettingType type, std::string text, const std::uint32_t formId, model::ValueOptions valueOptions) {
        return model::Setting{ .type = type,
            .text = std::move(text),
            .help = "Where the flashlight is.",
            .valueOptions = std::move(valueOptions),
            .storage = model::GlobalStorage{ .plugin = "MyMod.esp", .formId = formId } };
    };
    std::vector<std::string> many;
    for (int i = 1; i <= 20; ++i) {
        many.push_back(std::format("Option {}", i));
    }
    test_globals::TestGlobals globals;
    globals.values = { { { "MyMod.esp", 0x800 }, 0.0f }, { { "MyMod.esp", 0x801 }, 1.0f }, { { "MyMod.esp", 0x802 }, 2.0f }, { { "MyMod.esp", 0x803 }, 11.0f } };
    mcm::McmStore store(globals);

    model::Mod mod{ .name = "Choices", .displayName = "Choices", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(choice(SettingType::Stepper, "A stepper at its first option", 0x800, optionsOf({ "Low", "Medium", "High" })));
    mod.frontPage.items.push_back(choice(SettingType::Stepper, "A stepper at its second", 0x801, optionsOf({ "Low", "Medium", "High" })));
    mod.frontPage.items.push_back(choice(SettingType::Stepper, "A stepper at its last", 0x802, optionsOf({ "Low", "Medium", "High" })));
    mod.frontPage.items.push_back(
        choice(SettingType::Dropdown, "Three options", 0x801, optionsOf({ "Head", "Offhand", "A place with a name that is much longer than the list is wide" })));
    mod.frontPage.items.push_back(choice(SettingType::Dropdown, "Twenty options", 0x803, optionsOf(many)));
    mod.frontPage.items.push_back(choice(SettingType::Dropdown, "With no options", 0x800, {}));
    for (int i = 1; i <= 2; ++i) {
        mod.frontPage.items.push_back(setting(SettingType::Switcher, std::format("A row under the dropdowns {}", i)));
    }
    mod.frontPage.items.push_back(choice(SettingType::Dropdown, "At the bottom of the page", 0x801, optionsOf({ "Head", "Offhand", "Primary hand", "Chest" })));

    const auto panelOf = [&mod, &store] {
        FullPanel panel;
        panel.setStore(&store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = std::nullopt });
        return panel;
    };
    // where a control ends at the right, and the middle of a stepper's button to the option after
    const float right = test_panel::PANEL_WIDTH - test_panel::CONTENT_START - theme::SCROLLBAR_SIZE - theme::SPACE_2 - theme::CONTROL_HEIGHT - theme::SPACE_2;
    const float after = right - theme::CONTROL_HEIGHT * 0.5f;
    for (const bool pressed : { false, true }) {
        auto panel = panelOf();
        WARN("Saved " << renderToFile(panel, pressed ? "stepper-button-pressed" : "stepper-button-hover", ImVec2(after, pageRow(1).y), pressed));
    }

    // a list is opened by a press on its row, and then the ray is on an option of it. The list of the last
    // row opens over it
    const std::array lists{ std::pair{ "dropdown-open", 3 }, std::pair{ "dropdown-open-scrolls", 4 }, std::pair{ "dropdown-open-over", 8 } };
    for (const auto& [name, row] : lists) {
        auto panel = panelOf();
        const auto open = [&panel, row] {
            for (const bool held : { false, false, true, false, false }) {
                test_panel::drawFrame(panel, pageRow(row), held);
            }
        };
        const float optionY = row == 8 ? pageRow(row).y - theme::ROW_PITCH * 2.0f : pageRow(row).y + theme::ROW_PITCH * 2.0f;
        WARN("Saved " << renderToFile(panel, name, ImVec2(right - 150.0f, optionY), false, open));
    }
}

TEST_CASE("FullPanel render: the help line, with a help of each length", "[.render]")
{
    const std::array names{ "help-one-line", "help-two-lines", "help-three-lines", "help-cut", "help-name-alone" };
    for (int row = 0; row < static_cast<int>(names.size()); ++row) {
        auto panel = helpPanel();
        WARN("Saved " << renderToFile(panel, names[row], pageRow(row)));
    }
}

TEST_CASE("FullPanel render: a row of a page under the ray, and while the trigger is held on it", "[.render]")
{
    for (const bool pressed : { false, true }) {
        auto panel = testSetPanel();
        select(panel, "Sim Settlements 2", "Gameplay");
        WARN("Saved " << renderToFile(panel, pressed ? "page-row-pressed" : "page-row-hover", pageRow(2), pressed));
    }
}

TEST_CASE("FullPanel render: names too long for the title bar and for the tree", "[.render]")
{
    FullPanel panel;
    panel.setMods({ modOf("Sim Settlements 2 - All Chapters with the City Plan Contest Packs", { "Settlement Gameplay and City Plan Options of Chapter 2" }) });
    select(panel, "Sim Settlements 2 - All Chapters with the City Plan Contest Packs", "Settlement Gameplay and City Plan Options of Chapter 2");
    WARN("Saved " << renderToFile(panel, "long-names", POINTER_AWAY));
}
