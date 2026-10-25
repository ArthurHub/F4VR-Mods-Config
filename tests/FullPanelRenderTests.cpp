#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <imgui.h>

#include "TestClients.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestPanel.h"
#include "TestProperties.h"
#include "Theme.h"
#include "mcm/McmLoader.h"
#include "mcm/McmStore.h"
#include "model/TextMarkup.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"
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
     * ImGui's font texture, the opacity of its pixels read between them. The font's glyphs are white, with
     * their opacity in the last byte of a pixel.
     */
    struct FontTexture
    {
        const ImTextureData* texture = nullptr;

        float at(const int x, const int y) const
        {
            const int width = texture->Width;
            const int height = texture->Height;
            const auto* pixel = texture->Pixels + (std::clamp(y, 0, height - 1) * width + std::clamp(x, 0, width - 1)) * texture->BytesPerPixel;
            return pixel[texture->BytesPerPixel - 1] / 255.0f;
        }

        float sample(const float u, const float v) const
        {
            const float x = u * texture->Width - 0.5f;
            const float y = v * texture->Height - 0.5f;
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
     * inside the command's clip rectangle, at the frame's framebuffer scale. A command names the font's texture
     * by the texture, and an image of a mod's page by its pixels.
     */
    Image rasterize(const ImDrawData& drawData)
    {
        const float scale = drawData.FramebufferScale.x;
        Image image(static_cast<int>(std::lround(drawData.DisplaySize.x * scale)), static_cast<int>(std::lround(drawData.DisplaySize.y * scale)));
        for (const auto* commandList : drawData.CmdLists) {
            for (const auto& command : commandList->CmdBuffer) {
                if (command.UserCallback) {
                    continue;
                }
                const ImVec4 clip(command.ClipRect.x * scale, command.ClipRect.y * scale, command.ClipRect.z * scale, command.ClipRect.w * scale);
                const auto* indexes = commandList->IdxBuffer.Data + command.IdxOffset;
                const auto* vertexes = commandList->VtxBuffer.Data + command.VtxOffset;
                const FontTexture font{ .texture = command.TexRef._TexData };
                const auto* pixels = font.texture ? nullptr : reinterpret_cast<const model::ImagePixels*>(static_cast<std::uintptr_t>(command.TexRef.GetTexID()));
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
     * A texture of the config spec showcase, by its path under its Textures folder, as the game loads it: read from
     * its DDS file, whose pixels are not compressed, as blue, green, red and alpha from the top row. None for a
     * file that is not there or not of that form. A file is read once.
     */
    const model::ImagePixels* showcaseTexture(const std::string& texture)
    {
        static std::map<std::string, std::unique_ptr<model::ImagePixels>> textures;
        if (const auto found = textures.find(texture); found != textures.end()) {
            return found->second.get();
        }
        auto& kept = textures[texture];
        const auto file = std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path() / "Textures" / texture;
        std::ifstream stream(file, std::ios::binary);
        const std::vector<std::uint8_t> bytes{ std::istreambuf_iterator(stream), std::istreambuf_iterator<char>() };
        const auto read = [&bytes](const std::size_t at) {
            return static_cast<std::uint32_t>(bytes[at] | bytes[at + 1] << 8 | bytes[at + 2] << 16 | bytes[at + 3] << 24);
        };
        // where the header has the height, the width, and the bits of a pixel, which are 32 in a file that is
        // not compressed
        constexpr std::size_t HEADER_SIZE = 128;
        constexpr std::size_t HEIGHT_AT = 12;
        constexpr std::size_t WIDTH_AT = 16;
        constexpr std::size_t PIXEL_BITS_AT = 88;
        if (bytes.size() < HEADER_SIZE || !std::equal(bytes.begin(), bytes.begin() + 4, "DDS ") || read(PIXEL_BITS_AT) != 32) {
            return nullptr;
        }
        auto pixels = std::make_unique<model::ImagePixels>(model::ImagePixels{ .name = texture, .width = read(WIDTH_AT), .height = read(HEIGHT_AT), .rgba = {} });
        if (bytes.size() < HEADER_SIZE + std::size_t{ pixels->width } * pixels->height * 4) {
            return nullptr;
        }
        pixels->rgba.assign(bytes.begin() + HEADER_SIZE, bytes.begin() + HEADER_SIZE + std::size_t{ pixels->width } * pixels->height * 4);
        for (std::size_t at = 0; at < pixels->rgba.size(); at += 4) {
            std::swap(pixels->rgba[at], pixels->rgba[at + 2]);
        }
        kept = std::move(pixels);
        return kept.get();
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
        // an image is drawn with its pixels as its texture, which the rasterizer reads. A config spec's texture is
        // the config spec showcase's, and any other is not there
        panel.setImageDrawer({ .textureSize = [](const std::string& texture) -> std::optional<ImVec2> {
                                  const auto* pixels = showcaseTexture(texture);
                                  return pixels ? std::optional(ImVec2(static_cast<float>(pixels->width), static_cast<float>(pixels->height))) : std::nullopt;
                              },
            .draw =
                [](const model::Image& image, const ImVec2& min, const ImVec2& max) {
                    const auto* pixels = image.pixels ? image.pixels.get() : showcaseTexture(image.texture);
                    ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(pixels)), min, max);
                } });
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
        saveBmp(file, rasterize(*ImGui::GetDrawData()));
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
        tools.items.push_back(model::Hotkey{ .id = "openMenu", .text = "Open the menu" });
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
                                    return ui::QuickLaunchResult{};
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
        mod.frontPage.items.push_back(row("Brightness", "How bright the beam is. (Default: 1.00)"));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + "(Default: off)"));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + sentence + "(Default: off)"));
        mod.frontPage.items.push_back(row("Hide HUD in Pip-Boy", sentence + sentence + sentence + sentence + "(Default: off)"));
        mod.frontPage.items.push_back(row("A switcher with a name that is much too long for the room a row has before its control and ends in an ellipsis", {}));

        FullPanel panel;
        panel.setMods({ std::move(mod) });
        panel.select({ .mod = 0, .page = {} });
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
     * The places of a page of a mod on the way to it, by the names of the pages on that way:
     * { "Detailed Examples", "Files" } is the page Files under the page Detailed Examples.
     */
    std::vector<std::size_t> pagePlaces(const model::Mod& mod, const std::initializer_list<std::string_view> path)
    {
        std::vector<std::size_t> places;
        const auto* pages = &mod.pages;
        for (const auto name : path) {
            const auto found = std::ranges::find(*pages, name, &model::Page::name);
            REQUIRE(found != pages->end());
            places.push_back(static_cast<std::size_t>(found - pages->begin()));
            pages = &found->pages;
        }
        return places;
    }

    /**
     * A page of a mod by the names of the pages on the way to it, as `pagePlaces` has them.
     */
    const model::Page& findPage(const model::Mod& mod, const std::initializer_list<std::string_view> path)
    {
        const model::Page* page = nullptr;
        const auto* pages = &mod.pages;
        for (const auto place : pagePlaces(mod, path)) {
            page = &(*pages)[place];
            pages = &page->pages;
        }
        REQUIRE(page);
        return *page;
    }

    /**
     * Shows a mod of the panel by its name, and one of its pages by the names of the pages on the way to it.
     */
    void select(FullPanel& panel, const std::string_view mod, const std::initializer_list<std::string_view> path = {})
    {
        const auto& mods = panel.mods();
        const auto found = std::ranges::find(mods, mod, &model::Mod::displayName);
        REQUIRE(found != mods.end());
        panel.select({ .mod = static_cast<std::size_t>(found - mods.begin()), .page = pagePlaces(*found, path) });
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
    WARN("Saved " << renderToFile(panel, "front-page-quick-launch-hover", ImVec2(640.0f, 215.0f)));
}

TEST_CASE("FullPanel render: the quick launch section folded with the ray on its heading", "[.render]")
{
    auto panel = testSetPanel();
    const ImVec2 heading(640.0f, 171.0f);
    WARN("Saved " << renderToFile(panel, "front-page-quick-launch-folded", heading, false, [&panel, heading] {
        for (const bool pressed : { false, false, true, false }) {
            test_panel::drawFrame(panel, heading, pressed);
        }
    }));
}

TEST_CASE("FullPanel render: no mods and no quick launch", "[.render]")
{
    FullPanel panel;
    WARN("Saved " << renderToFile(panel, "front-page-empty", POINTER_AWAY));
}

TEST_CASE("FullPanel render: mods that are not available, in the tree and in the notices, with the ray on one", "[.render]")
{
    const auto notAvailable = [](const std::string& name, const std::string& reason) {
        return model::Mod{ .name = name, .displayName = name, .frontPage = {}, .pages = {}, .notAvailable = reason };
    };
    std::vector<model::Mod> mods;
    mods.push_back(modOf("XDI"));
    mods.push_back(modOf("Chemfluence"));
    mods.push_back(notAvailable("Sim Settlements 2", "its plugin SS2.esm is not loaded"));
    mods.push_back(notAvailable("BrokenConfig", "its config cannot be read"));
    mods.push_back(modOf("Workshop Framework", { "Hotkeys", "Settlement Gameplay" }));

    FullPanel panel;
    panel.setMods(std::move(mods));
    panel.setQuickLaunch({ { .name = "FRIK", .help = "Opens FRIK's own configuration in the world.", .open = [] {
                                return ui::QuickLaunchResult{};
                            } } });

    // the third row of the tree, which is Sim Settlements 2's in the order of the names
    const float rowsTop = test_panel::CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3 + theme::SPACE_2 + 40.0f + theme::SPACE_1;
    const ImVec2 onRow(test_panel::CONTENT_START + theme::SPACE_2 + 100.0f, rowsTop + 2.0f * (theme::CONTROL_HEIGHT + theme::SPACE_1) + theme::CONTROL_HEIGHT * 0.5f);
    WARN("Saved " << renderToFile(panel, "mods-not-available", onRow));
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
    select(panel, "Sim Settlements 2", { "Gameplay" });
    WARN("Saved " << renderToFile(panel, "mod-page", ImVec2(360.0f, 50.0f)));
}

TEST_CASE("FullPanel render: the Help page over a page of a mod, with the ray on Help", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Sim Settlements 2", { "Gameplay" });
    // the shipped config's buttons and bindings
    panel.setControls(ui::menuControls({
        .openMenu = { .hand = vrcf::Hand::Primary,
            .type = vrcf::ActivationType::LongPress,
            .button = vr::k_EButton_SteamVR_Touchpad,
            .modifier = vrcf::InputModifier{ .button = vr::k_EButton_SteamVR_Touchpad, .hand = vrcf::Hand::Offhand },
            .duration = 1.5f },
        .back = vr::k_EButton_Grip,
        .move = vr::k_EButton_Grip,
        .recenter = { .hand = vrcf::Hand::Offhand, .type = vrcf::ActivationType::LongPress, .button = vr::k_EButton_SteamVR_Touchpad },
        .leftHanded = false,
    }));
    // Help, the third button from the right of the title bar
    const ImVec2 help(test_panel::PANEL_WIDTH - test_panel::CONTENT_START - theme::SPACE_2 * 3.0f - theme::CONTROL_HEIGHT * 2.5f,
        test_panel::CONTENT_START + theme::TITLEBAR_HEIGHT * 0.5f);
    WARN("Saved " << renderToFile(panel, "help-page", help, false, [&panel, help] {
        for (const bool pressed : { false, false, true, false }) {
            test_panel::drawFrame(panel, help, pressed);
        }
    }));
}

TEST_CASE("FullPanel render: a page under the tree's last rows is scrolled to", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "Workshop Framework", { "Advanced Controls" });
    WARN("Saved " << renderToFile(panel, "mod-page-scrolled-to", POINTER_AWAY));
}

TEST_CASE("FullPanel render: pages that are set in under another by spaces", "[.render]")
{
    auto panel = testSetPanel();
    select(panel, "FallUI - Inventory", { "    Container/Barter/Companion" });
    WARN("Saved " << renderToFile(panel, "mod-page-set-in", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the pages of the MCM showcase", "[.render]")
{
    // the showcase is a Data folder, installed in a game's folder, which its dropdownFiles list files from
    const test_files::TempFolder game;
    std::filesystem::copy(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path().parent_path(), game.path / "Data", std::filesystem::copy_options::recursive);
    const auto mods = mcm::loadConfigs(game.path / "Data" / "MCM" / "Config");
    REQUIRE(mods.size() == 1);

    // the showcase has no game plugin, so none of its globals is in the game. Its ModSettings are its defaults
    test_globals::TestGlobals globals;
    mcm::McmStore store(game.path / "Data" / "MCM");

    // a panel for each image: a panel sets its style once, on the ImGui context it is first drawn in
    const auto render = [&mods, &store](std::vector<std::size_t> page, const std::string& name) {
        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods(mods);
        panel.select({ .mod = 0, .page = std::move(page) });
        WARN("Saved " << renderToFile(panel, name, POINTER_AWAY));
    };
    render({}, "mcm-showcase-front-page");
    for (std::size_t page = 0; page < mods[0].pages.size(); ++page) {
        render({ page }, std::format("mcm-showcase-page-{}", page + 1));
    }
}

TEST_CASE("FullPanel render: the pages of the mod's own config spec, with the ray on the row of a page with a help", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    // its values are the shipped config's, in a My Games folder of its own
    const test_files::TempFolder myGames;
    const auto configFolder = myGames.path / "Mods_Config" / "ModsConfigVR";
    std::filesystem::create_directories(configFolder);
    std::filesystem::copy_file(MODS_CONFIG_SHIPPED_CONFIG, configFolder / "ModsConfigVR.toml");
    spec::ConfigSpecStore store({}, myGames.path);

    // a panel for each image: a panel sets its style once, on the ImGui context it is first drawn in
    const auto render = [&mods, &store](std::vector<std::size_t> page, const std::string& name, const ImVec2& pointer) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        panel.select({ .mod = 0, .page = std::move(page) });
        WARN("Saved " << renderToFile(panel, name, pointer));
    };
    // the second row of the tree, which is the config spec's first page
    const float rowsTop = test_panel::CONTENT_START + theme::TITLEBAR_HEIGHT + theme::SPACE_3 + theme::SPACE_2 + 40.0f + theme::SPACE_1;
    const ImVec2 onPage(test_panel::CONTENT_START + theme::SPACE_2 + 100.0f, rowsTop + theme::CONTROL_HEIGHT + theme::SPACE_1 + theme::CONTROL_HEIGHT * 0.5f);
    render({}, "config-spec-front-page", onPage);
    for (std::size_t page = 0; page < mods[0].pages.size(); ++page) {
        render({ page }, std::format("config-spec-page-{}", page + 1), POINTER_AWAY);
    }
}

TEST_CASE("FullPanel render: hidden and disabled pages", "[.render]")
{
    const auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [ { "type": "text", "text": "Page conditions affect navigation and are inherited by child pages." } ],
        "pages": [
            { "pageDisplayName": "Conditional", "help": "Available when the mod is ready.",
              "visibleWhen": { "clientState": "visible" }, "enabledWhen": { "clientState": "ready" },
              "content": [ { "type": "text", "text": "The conditional page is enabled." } ],
              "pages": [ { "pageDisplayName": "Child", "content": [ { "type": "text", "text": "Child inherits the page conditions." } ] } ] },
            { "pageDisplayName": "Always available", "content": [ { "type": "text", "text": "Always available." } ] }
        ]
    })");
    REQUIRE(mod);
    spec::ConfigSpecStore store({}, {});
    bool visible = true;
    bool ready = false;
    store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "visible",
                                                  [&] {
                                                      return std::optional<model::Value>(visible);
                                                  } },
                                      { "ready",
                                          [&] {
                                              return std::optional<model::Value>(ready);
                                          } } } }
            .calls());
    const auto render = [&](const std::vector<std::size_t>& page, const std::string& name, const ImVec2 pointer = POINTER_AWAY) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ *mod });
        panel.select({ .mod = 0, .page = page });
        WARN("Saved " << renderToFile(panel, name, pointer));
    };
    render({}, "pages-disabled", ImVec2(200.0f, 330.0f));
    visible = false;
    render({}, "pages-hidden");
    visible = true;
    ready = true;
    render({ 0, 0 }, "pages-enabled-child");
}

TEST_CASE("FullPanel render: client states in text", "[.render]")
{
    const auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [
            { "type": "text", "text": "<font size='30'><b>Version {clientState:version}</b></font>", "html": true },
            { "type": "text", "text": "<b>Status:</b> <font color='#FFBF33'>{clientState:status}</font>", "html": true },
            { "type": "text", "text": "Items: {clientState:count}   Scale: {clientState:scale}   Ready: {clientState:ready}" },
            { "type": "text", "text": "Literal state value: {clientState:literal}" },
            { "type": "text", "text": "Unregistered state: {clientState:missing}" }
        ]
    })");
    REQUIRE(mod);
    spec::ConfigSpecStore store({}, {});
    std::string version = "1.2.3";
    std::string status = "Ready to configure";
    store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "version",
                                                  [&] {
                                                      return std::optional<model::Value>(version);
                                                  } },
                                      { "status",
                                          [&] {
                                              return std::optional<model::Value>(status);
                                          } },
                                      { "count",
                                          [] {
                                              return std::optional<model::Value>(12);
                                          } },
                                      { "scale",
                                          [] {
                                              return std::optional<model::Value>(1.25f);
                                          } },
                                      { "ready",
                                          [] {
                                              return std::optional<model::Value>(true);
                                          } },
                                      { "literal",
                                          [] {
                                              return std::optional<model::Value>("<b>not markup</b> & {clientState:version}"s);
                                          } } } }
            .calls());
    const auto render = [&mod](spec::ConfigSpecStore& source, const std::string& name) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &source);
        panel.setMods({ *mod });
        panel.select({ .mod = 0 });
        WARN("Saved " << renderToFile(panel, name, POINTER_AWAY));
    };
    render(store, "client-text-values");
    version = "1.2.4";
    status = "Calibration complete.\nYour settings are ready.";
    render(store, "client-text-updated");
    spec::ConfigSpecStore alone({}, {});
    render(alone, "client-text-unavailable");
}

TEST_CASE("FullPanel render: the pages of the config spec showcase", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    // its files are in the Data folder it is, and its file of My Games is not there
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);

    // a panel for each image: a panel sets its style once, on the ImGui context it is first drawn in
    const auto render = [&mods, &store](const std::vector<std::size_t>& page, const std::string& name) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        panel.select({ .mod = 0, .page = page });
        WARN("Saved " << renderToFile(panel, name, POINTER_AWAY));
    };
    render({}, "config-spec-showcase-front-page");
    // each page, and the pages under it, as config-spec-showcase-page-7-1-1
    std::vector<std::size_t> page;
    const auto renderPages = [&render, &page](this const auto& self, const std::vector<model::Page>& pages) -> void {
        for (std::size_t place = 0; place < pages.size(); ++place) {
            page.push_back(place);
            std::string name = "config-spec-showcase-page";
            for (const auto at : page) {
                name += std::format("-{}", at + 1);
            }
            render(page, name);
            self(pages[place].pages);
            page.pop_back();
        }
    };
    renderPages(mods[0].pages);
}

TEST_CASE("FullPanel render: the Most Used page of the config spec showcase with values that wait for a Save", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(mods);
    panel.select({ .mod = 0, .page = pagePlaces(mods[0], { "Most Used" }) });

    // a switcher, a slider, a stepper, a dropdown and a text input of its Most Used page, each changed
    const auto change = [&store](const std::vector<model::Item>& items, const auto& self) -> void {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                self(group->items, self);
                continue;
            }
            const auto* setting = std::get_if<model::Setting>(&item);
            if (!setting) {
                continue;
            }
            const auto value = store.value(*setting).value;
            switch (setting->type) {
            case model::SettingType::Switcher:
                store.set(*setting, !std::get<bool>(value));
                break;
            case model::SettingType::Slider:
                store.set(*setting, setting->valueOptions.min);
                break;
            case model::SettingType::Stepper:
            case model::SettingType::Dropdown:
                store.set(*setting, setting->valueOptions.options.back().value);
                break;
            case model::SettingType::TextInput:
                // a number inside its limits, where it has them, so the value waits for a Save and is not invalid
                if (setting->valueType == model::StoredType::String) {
                    store.set(*setting, model::Value("Changed"));
                } else {
                    const auto& limits = setting->valueOptions;
                    store.set(*setting, model::Value(limits.max > limits.min ? std::format("{}", limits.max) : std::string("7")));
                }
                break;
            }
        }
    };
    change(findPage(panel.mods()[0], { "Most Used" }).items, change);
    REQUIRE(store.dirtyCount() > 0);
    WARN("Saved " << renderToFile(panel, "config-spec-showcase-pending", POINTER_AWAY));

    // the dialog that asks before the menu is closed, with the ray on Discard
    panel.close();
    REQUIRE(panel.isAsking());
    WARN("Saved " << renderToFile(panel, "config-spec-showcase-close-dialog", ImVec2(test_panel::PANEL_WIDTH * 0.5f + 140.0f, test_panel::PANEL_HEIGHT * 0.5f + 60.0f)));

    // and one with an invalid change, which disables Save, before the menu's front page
    const auto numberInput = [](const std::vector<model::Item>& items, const auto& self) -> const model::Setting* {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                if (const auto* found = self(group->items, self)) {
                    return found;
                }
            } else if (const auto* setting = std::get_if<model::Setting>(&item);
                       setting && setting->type == model::SettingType::TextInput && setting->valueType != model::StoredType::String) {
                return setting;
            }
        }
        return nullptr;
    };
    const auto* input = numberInput(findPage(panel.mods()[0], { "Most Used" }).items, numberInput);
    REQUIRE(input);
    store.set(*input, "not a number"s);
    REQUIRE(store.invalidCount() > 0);
    panel.back();
    panel.select({});
    REQUIRE(panel.isAsking());
    WARN("Saved " << renderToFile(panel, "config-spec-showcase-leave-dialog-invalid", POINTER_AWAY));
}

TEST_CASE("FullPanel render: a Save that failed, with a short reason and with one that wraps", "[.render]")
{
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "MyMod" / "MyMod.toml";
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nbEnable = false\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Enable", "default": false, "storage": { "path": ["Main", "bEnable"] } } ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select({ .mod = 0, .page = {} });
    store.set(std::get<model::Setting>(panel.mods()[0].frontPage.items[0]), true);

    {
        const test_files::ReadOnly readOnly(file);
        REQUIRE_FALSE(store.save());
    }
    WARN("Saved " << renderToFile(panel, "config-spec-save-failed", POINTER_AWAY));

    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main\r\nbEnable = false\r\n");
    REQUIRE_FALSE(store.save());
    WARN("Saved " << renderToFile(panel, "config-spec-save-failed-long", POINTER_AWAY));
}

TEST_CASE("FullPanel render: settings that take effect after a restart, their status before and after the Save, and the notice of the menu's front page", "[.render]")
{
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "Immersive Flashlight VR", "applies": "save",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "stepper", "valueType": "int", "id": "shadowSize", "text": "Shadow size", "help": "How sharp the beam's shadows are. Larger costs frame time.",
              "default": 2048, "applies": "restart", "valueOptions": { "options": [ { "value": 1024, "text": "1024" }, { "value": 2048, "text": "2048" },
              { "value": 4096, "text": "4096" } ] }, "storage": { "path": ["Main", "iShadowSize"] } },
            { "type": "slider", "valueType": "float", "text": "Brightness", "default": 1.0, "valueOptions": { "min": 0.0, "max": 2.0, "step": 0.05 },
              "storage": { "path": ["Main", "fBrightness"] } },
            { "type": "switcher", "text": "A long name of a setting that needs a restart and warns", "default": false, "applies": "restart",
              "warnings": [ { "when": { "setting": "shadowSize", "equals": 4096 }, "text": "Costly." } ], "storage": { "path": ["Main", "bLong"] } }
        ]
    })");
    REQUIRE(mod);
    std::vector<model::Mod> mods;
    mods.push_back(std::move(*mod));
    mods.push_back(model::Mod{ .name = "SS2", .displayName = "Sim Settlements 2", .frontPage = {}, .pages = {}, .notAvailable = "its plugin SS2.esm is not loaded" });
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(std::move(mods));
    panel.select({ .mod = 0, .page = {} });
    const auto& items = panel.mods()[0].frontPage.items;
    store.set(std::get<model::Setting>(items[0]), 4096);
    store.set(std::get<model::Setting>(items[1]), 1.25f);

    WARN("Saved " << renderToFile(panel, "config-spec-restart-row", pageRow(0)));
    WARN("Saved " << renderToFile(panel, "config-spec-restart-pending", POINTER_AWAY));
    REQUIRE(store.save());
    WARN("Saved " << renderToFile(panel, "config-spec-restart-saved", POINTER_AWAY));
    panel.select({});
    WARN("Saved " << renderToFile(panel, "front-page-restart-notice", POINTER_AWAY));

    // the Notices heading is under the page's two lines of text: the panel has no quick launch
    const ImVec2 notices(640.0f, 171.0f);
    WARN("Saved " << renderToFile(panel, "front-page-notices-folded", notices, false, [&panel, notices] {
        for (const bool pressed : { false, false, true, false }) {
            test_panel::drawFrame(panel, notices, pressed);
        }
    }));
}

TEST_CASE("FullPanel render: values the player typed that are not valid, which disable Save, and a text with a most of characters as it is typed", "[.render]")
{
    const test_files::TempFolder myGames;
    myGames.write(R"(Mods_Config\MyMod\MyMod.toml)", "[Main]\r\nsName = \"Standing\"\r\nfHeight = 1.78\r\niCount = 3\r\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "textinput", "text": "Profile name", "help": "Type a name with the keyboard.", "default": "Standing",
              "valueOptions": { "allowEmpty": false, "maxLength": 16 }, "storage": { "path": ["Main", "sName"] } },
            { "type": "textinput", "valueType": "float", "text": "Player height", "help": "A numeric input between 0.5 and 2.5 meters.", "default": 1.78,
              "valueOptions": { "min": 0.5, "max": 2.5 }, "display": { "decimals": 2, "unit": "m" }, "storage": { "path": ["Main", "fHeight"] } },
            { "type": "textinput", "valueType": "int", "text": "Count", "default": 3, "storage": { "path": ["Main", "iCount"] } }
        ]
    })");
    REQUIRE(mod);

    // the ray on a row that is invalid, which has the reason, and away, where the status counts them
    for (const bool onRow : { true, false }) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods({ *mod });
        panel.select({ .mod = 0, .page = {} });
        const auto& items = panel.mods()[0].frontPage.items;
        store.set(std::get<model::Setting>(items[0]), ""s);
        store.set(std::get<model::Setting>(items[1]), "3"s);
        store.set(std::get<model::Setting>(items[2]), "4.5"s);
        REQUIRE(store.invalidCount() == 3);
        WARN("Saved " << renderToFile(panel, onRow ? "config-spec-invalid-typed" : "config-spec-invalid-typed-status", onRow ? pageRow(1) : POINTER_AWAY));
        store.discard();
    }

    // the name is typed past its most, which keeps only what fits
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ *mod });
    panel.select({ .mod = 0, .page = {} });
    const auto edit = [&panel] {
        for (const bool held : { false, false, true, false, false }) {
            test_panel::drawFrame(panel, pageRow(0), held);
        }
        ImGui::GetIO().AddInputCharactersUTF8(" and seated");
    };
    WARN("Saved " << renderToFile(panel, "config-spec-text-input-most", POINTER_AWAY, false, edit));
}

TEST_CASE("FullPanel render: the values of the config spec showcase's file that are not the settings'", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(mods);
    select(panel, "Mods Config VR: Showcase", { "Detailed Examples", "Files" });

    // they are at the end of the page, and the help line has the reason of the last
    const auto scrollToEnd = [&panel] {
        test_panel::drawFrame(panel, pageRow(3));
        test_panel::drawFrame(panel, pageRow(3));
        ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
        test_panel::drawFrame(panel, pageRow(3));
        test_panel::drawFrame(panel, pageRow(7));
    };
    WARN("Saved " << renderToFile(panel, "config-spec-showcase-invalid", POINTER_AWAY, false, scrollToEnd));
}

TEST_CASE("FullPanel render: the ends of the config spec showcase's pages of rows that hold no value, and of its textures that are not there", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);

    // the images are at the end of each page: the thumbstick scrolls the page the ray is on
    for (const auto& [pageName, name] : { std::pair{ "Images", "config-spec-showcase-images" }, std::pair{ "Mistakes", "config-spec-showcase-missing-textures" } }) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        select(panel, "Mods Config VR: Showcase", { "Detailed Examples", pageName });
        const auto scrollToEnd = [&panel] {
            test_panel::drawFrame(panel, pageRow(3));
            test_panel::drawFrame(panel, pageRow(3));
            ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
            test_panel::drawFrame(panel, pageRow(3));
        };
        WARN("Saved " << renderToFile(panel, name, POINTER_AWAY, false, scrollToEnd));
    }
}

TEST_CASE("FullPanel render: the ends of the config spec showcase's pages of numbers, of read-only settings and of buttons, with the ray on a row", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);

    // the percentages and the numbers too wide for their box, with the help line of the last, the read-only
    // settings with no value, with the help line of one, and the button the menu does not run, with its reason
    for (const auto& [pageName, name, row] : { std::tuple{ "Numbers", "config-spec-showcase-numbers-end", 8 },
             std::tuple{ "Read-only", "config-spec-showcase-read-only-end", 6 },
             std::tuple{ "Buttons", "config-spec-showcase-buttons-end", 8 } }) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        select(panel, "Mods Config VR: Showcase", { "Detailed Examples", pageName });
        const auto scrollToEnd = [&panel] {
            test_panel::drawFrame(panel, pageRow(3));
            test_panel::drawFrame(panel, pageRow(3));
            ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
            test_panel::drawFrame(panel, pageRow(3));
        };
        WARN("Saved " << renderToFile(panel, name, pageRow(row), false, scrollToEnd));
    }
}

TEST_CASE("FullPanel render: the config spec showcase's warnings, as shipped and with Shadows on, with the ray on a row that warns", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    for (const bool shadows : { false, true }) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        select(panel, "Mods Config VR: Showcase", { "Detailed Examples", "Warnings" });
        // Shadows is set once its session is open, which the page's first frame opens
        const auto turnOn = [&panel, &store] {
            test_panel::drawFrame(panel, POINTER_AWAY);
            const auto& page = findPage(panel.mods()[0], { "Detailed Examples", "Warnings" });
            store.set(std::get<model::Setting>(std::get<model::Group>(page.items[0]).items[1]), true);
        };
        const auto name = shadows ? "config-spec-showcase-warnings-shadows" : "config-spec-showcase-warnings";
        WARN("Saved " << renderToFile(panel, name, pageRow(shadows ? 4 : 1), false, shadows ? std::function<void()>(turnOn) : std::function<void()>()));
    }
}

TEST_CASE("FullPanel render: a config spec's section in a section, with rows before and after it", "[.render]")
{
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Enable", "storage": { "path": ["Main", "bEnable"] } },
            { "type": "section", "text": "Holsters", "content": [
                { "type": "switcher", "text": "Enable holsters", "storage": { "path": ["Holsters", "bEnabled"] } },
                { "type": "section", "text": "Advanced", "content": [
                    { "type": "slider", "text": "Reach", "storage": { "path": ["Holsters", "fReach"] } },
                    { "type": "section", "text": "Even more", "content": [
                        { "type": "slider", "text": "Angle", "storage": { "path": ["Holsters", "fAngle"] } }
                    ] }
                ] },
                { "type": "slider", "text": "Size", "storage": { "path": ["Holsters", "fSize"] } }
            ] },
            { "type": "section", "text": "Body", "content": [
                { "type": "slider", "text": "Height", "storage": { "path": ["Body", "fHeight"] } }
            ] }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setMods({ std::move(*mod) });
    panel.select({ .mod = 0, .page = {} });
    WARN("Saved " << renderToFile(panel, "config-spec-nested-sections", POINTER_AWAY));
    // the ray on the row under no heading, whose frame starts where a heading's row does: a panel of its own, as
    // a panel sets ImGui's style as its mod is selected
    FullPanel pointedPanel;
    pointedPanel.setMods(panel.mods());
    pointedPanel.select({ .mod = 0, .page = {} });
    WARN("Saved " << renderToFile(pointedPanel, "config-spec-top-level-row-hover", pageRow(0)));
}

TEST_CASE("FullPanel render: a config spec's sections that fold, folded with what they hide and open, with the ray on one", "[.render]")
{
    const test_files::TempFolder myGames;
    myGames.write("MyMod.toml", "[Holsters]\nfReach = 99.0\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
            { "type": "section", "text": "Holsters", "content": [
                { "type": "switcher", "text": "Enable holsters", "default": true, "storage": { "path": ["Holsters", "bEnabled"] } },
                { "type": "section", "text": "Advanced", "help": "Fine-tuning of the holsters.", "isCollapsible": true, "collapsed": true, "content": [
                    { "type": "slider", "text": "Reach", "default": 10.0, "valueOptions": { "min": 0, "max": 50, "step": 1 }, "storage": { "path": ["Holsters", "fReach"] } },
                    { "type": "slider", "text": "Angle", "default": 45.0, "valueOptions": { "min": 0, "max": 90, "step": 5 }, "storage": { "path": ["Holsters", "fAngle"] } },
                    { "type": "switcher", "text": "Snap", "default": false, "storage": { "path": ["Holsters", "bSnap"] } }
                ] },
                { "type": "slider", "text": "Size", "default": 1.0, "valueOptions": { "min": 0.5, "max": 2, "step": 0.1 }, "storage": { "path": ["Holsters", "fSize"] } }
            ] },
            { "type": "section", "text": "Weapons", "isCollapsible": true, "content": [
                { "type": "switcher", "text": "Draw with the grip", "default": true, "storage": { "path": ["Weapons", "bGrip"] } },
                { "type": "section", "text": "Even more", "isCollapsible": true, "collapsed": true, "content": [
                    { "type": "switcher", "text": "Hidden", "default": false, "storage": { "path": ["Weapons", "bHidden"] } }
                ] }
            ] },
            { "type": "section", "text": "Body", "isCollapsible": true, "collapsed": true, "content": [
                { "type": "slider", "text": "Height", "default": 1.0, "valueOptions": { "min": 0.5, "max": 2, "step": 0.1 }, "storage": { "path": ["Body", "fHeight"] } }
            ] }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select({ .mod = 0, .page = {} });

    // Advanced hides Reach, which the file has outside its range, and two changes that wait for a Save
    const auto& holsters = std::get<model::Group>(panel.mods()[0].frontPage.items[1]);
    const auto& advanced = std::get<model::Group>(holsters.items[1]);
    REQUIRE(store.set(std::get<model::Setting>(advanced.items[1]), 60.0f));
    REQUIRE(store.set(std::get<model::Setting>(advanced.items[2]), true));

    // the heading of Advanced, after the heading of Holsters, which is not as high as a row, and its first row
    const ImVec2 advancedHeading(pageRow(2).x, pageRow(2).y + 45.0f);
    WARN("Saved " << renderToFile(panel, "config-spec-folding-sections", advancedHeading));
}

TEST_CASE("FullPanel render: a config spec's section in a section, with its line down its rows, their marks, and a folded section in it", "[.render]")
{
    const test_files::TempFolder myGames;
    myGames.write("MyMod.toml", "[Holsters]\nfReach = 99.0\n");
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "section", "text": "Holsters", "content": [
                { "type": "switcher", "text": "Enable holsters", "default": true, "storage": { "path": ["Holsters", "bEnabled"] } },
                { "type": "section", "text": "Left hip", "content": [
                    { "type": "text", "text": "Where the holster sits on the left hip." },
                    { "type": "slider", "text": "Reach", "default": 10.0, "valueOptions": { "min": 0, "max": 50, "step": 1 }, "storage": { "path": ["Holsters", "fReach"] } },
                    { "type": "slider", "text": "Down", "default": 12.0, "valueOptions": { "min": 0, "max": 50, "step": 1 }, "storage": { "path": ["Holsters", "fDown"] } },
                    { "type": "section", "text": "Advanced", "isCollapsible": true, "collapsed": true, "content": [
                        { "type": "switcher", "text": "Snap", "default": false, "storage": { "path": ["Holsters", "bSnap"] } }
                    ] }
                ] },
                { "type": "slider", "text": "Size", "default": 1.0, "valueOptions": { "min": 0.5, "max": 2, "step": 0.1 }, "storage": { "path": ["Holsters", "fSize"] } }
            ] },
            { "type": "section", "text": "Weapons", "content": [
                { "type": "switcher", "text": "Draw with the grip", "default": true, "storage": { "path": ["Weapons", "bGrip"] } },
                { "type": "section", "text": "Advanced", "isCollapsible": true, "content": [
                    { "type": "slider", "text": "Draw angle", "default": 35.0, "valueOptions": { "min": 0, "max": 90, "step": 5 }, "storage": { "path": ["Weapons", "fAngle"] } }
                ] }
            ] }
        ]
    })");
    REQUIRE(mod);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select({ .mod = 0, .page = {} });

    // Reach is invalid in the file, and Down and Snap wait for a Save
    const auto& holsters = std::get<model::Group>(panel.mods()[0].frontPage.items[0]);
    const auto& leftHip = std::get<model::Group>(holsters.items[1]);
    REQUIRE(store.set(std::get<model::Setting>(leftHip.items[2]), 20.0f));
    REQUIRE(store.set(std::get<model::Setting>(std::get<model::Group>(leftHip.items[3]).items[0]), true));
    WARN("Saved " << renderToFile(panel, "config-spec-section-lines", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the rows of a group of the MCM showcase, while the switcher that controls it is on", "[.render]")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    test_globals::TestGlobals globals;
    mcm::McmStore store(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path());

    // off, as the group is when the mod is selected, and on
    for (const bool on : { false, true }) {
        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods(mods);
        const auto& pages = panel.mods()[0].pages;
        const auto page = std::ranges::find(pages, "Switchers", &model::Page::name);
        REQUIRE(page != pages.end());
        panel.select({ .mod = 0, .page = { static_cast<std::size_t>(page - pages.begin()) } });

        const auto& rows = std::get<model::Group>(page->items.back()).items;
        const auto unfold = std::ranges::find_if(rows, [](const model::Item& row) {
            const auto* setting = std::get_if<model::Setting>(&row);
            return setting && setting->text == "Unfold settings";
        });
        REQUIRE(unfold != rows.end());
        REQUIRE(store.set(std::get<model::Setting>(*unfold), on));

        // the group is at the end of the page: the thumbstick scrolls the page the ray is on
        const auto scrollToEnd = [&panel] {
            test_panel::drawFrame(panel, pageRow(3));
            test_panel::drawFrame(panel, pageRow(3));
            ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
            test_panel::drawFrame(panel, pageRow(3));
        };
        WARN("Saved " << renderToFile(panel, on ? "mcm-showcase-group-on" : "mcm-showcase-group-off", POINTER_AWAY, false, scrollToEnd));
    }
}

TEST_CASE("FullPanel render: the dropdowns of files of the MCM showcase", "[.render]")
{
    // installed in a game's folder, which they list files from
    const test_files::TempFolder game;
    std::filesystem::copy(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path().parent_path(), game.path / "Data", std::filesystem::copy_options::recursive);
    const auto mods = mcm::loadConfigs(game.path / "Data" / "MCM" / "Config");
    REQUIRE(mods.size() == 1);
    test_globals::TestGlobals globals;
    mcm::McmStore store(game.path / "Data" / "MCM");

    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods(mods);
    const auto& pages = panel.mods()[0].pages;
    const auto page = std::ranges::find(pages, "Steppers and dropdowns", &model::Page::name);
    REQUIRE(page != pages.end());
    panel.select({ .mod = 0, .page = { static_cast<std::size_t>(page - pages.begin()) } });

    // they are at the end of the page: the thumbstick scrolls the page the ray is on
    const auto scrollToEnd = [&panel] {
        test_panel::drawFrame(panel, pageRow(3));
        test_panel::drawFrame(panel, pageRow(3));
        ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
        test_panel::drawFrame(panel, pageRow(3));
    };
    WARN("Saved " << renderToFile(panel, "mcm-showcase-dropdown-files", POINTER_AWAY, false, scrollToEnd));
}

TEST_CASE("FullPanel render: the choices of the config spec showcase that read their options in the game", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    test_properties::TestProperties lists;
    lists.lists[{ "Fallout4.esm", 0x1D98E2, "", "" }] = { "Abernathy Farm", "Cabot House", "The Castle", "Covenant", "Diamond City", "Drumlin Diner" };
    lists.lists[{ "Fallout4.esm", 0x78591, "", "" }] = { "Cait", "Codsworth", "Curie", "Paladin Danse", "Deacon", "John Hancock" };
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(mods);
    const auto& examples = panel.mods()[0].pages;
    const auto detailed = std::ranges::find(examples, "Detailed Examples", &model::Page::name);
    REQUIRE(detailed != examples.end());
    const auto page = std::ranges::find(detailed->pages, "Choices", &model::Page::name);
    REQUIRE(page != detailed->pages.end());
    panel.select({ .mod = 0, .page = { static_cast<std::size_t>(detailed - examples.begin()), static_cast<std::size_t>(page - detailed->pages.begin()) } });

    // they are at the end of the page: the thumbstick scrolls the page the ray is on
    const auto scrollToEnd = [&panel] {
        test_panel::drawFrame(panel, pageRow(3));
        test_panel::drawFrame(panel, pageRow(3));
        ImGui::GetIO().AddMouseWheelEvent(0.0f, -100.0f);
        test_panel::drawFrame(panel, pageRow(3));
    };
    WARN("Saved " << renderToFile(panel, "config-spec-showcase-game-options", POINTER_AWAY, false, scrollToEnd));
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
    mcm::McmStore store;

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
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = {} });
        WARN("Saved " << renderToFile(panel, onRow ? "values-not-available" : "values", onRow ? pageRow(5) : POINTER_AWAY));
    }
}

TEST_CASE("FullPanel render: the Reset of the rows with a default, disabled on those that are at it", "[.render]")
{
    using model::SettingType;
    using Type = model::StoredType;
    const auto modSetting = [](const SettingType type, std::string text, std::string name, const Type valueType, model::ValueOptions valueOptions = {}) {
        return model::Setting{ .type = type,
            .text = std::move(text),
            .help = "What the setting does.",
            .valueOptions = std::move(valueOptions),
            .storage = model::ModSettingStorage{ .name = std::move(name), .type = valueType } };
    };
    const test_files::TempFolder files;
    files.write("Config/Defaults/settings.ini", "[Main]\nbOn=1\nbSame=1\nfStrength=1.0\nfSame=1.0\niHours=1\niMode=1\niSame=1\n");
    files.write("Settings/Defaults.ini", "[Main]\nbOn=0\nfStrength=1.300000\niHours=2\niMode=2\n");
    mcm::McmStore store(files.path);

    const model::ValueOptions range{ .min = 0.0f, .max = 2.0f, .step = 0.1f, .options = {} };
    model::Mod mod{ .name = "Defaults", .displayName = "Defaults", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(modSetting(SettingType::Switcher, "A switcher that is changed", "bOn:Main", Type::Bool));
    mod.frontPage.items.push_back(modSetting(SettingType::Switcher, "A switcher at its default", "bSame:Main", Type::Bool));
    mod.frontPage.items.push_back(modSetting(SettingType::Slider, "A slider that is changed", "fStrength:Main", Type::Float, range));
    mod.frontPage.items.push_back(modSetting(SettingType::Slider, "A slider at its default", "fSame:Main", Type::Float, range));
    mod.frontPage.items.push_back(modSetting(SettingType::Stepper, "A stepper that is changed", "iHours:Main", Type::Int, optionsOf({ "2", "4", "8" })));
    mod.frontPage.items.push_back(modSetting(SettingType::Dropdown, "A dropdown that is changed", "iMode:Main", Type::Int, optionsOf({ "Spot", "Flood", "Lantern" })));
    mod.frontPage.items.push_back(modSetting(SettingType::Dropdown, "A dropdown at its default", "iSame:Main", Type::Int, optionsOf({ "Spot", "Flood", "Lantern" })));

    // with the ray on the row of the slider that is changed, on its Reset, and with the trigger held on it.
    // And with the trigger held on the Reset of the slider at its default, which is disabled
    const float reset = test_panel::PANEL_WIDTH - test_panel::CONTENT_START - theme::SCROLLBAR_SIZE - theme::SPACE_2 - theme::CONTROL_HEIGHT * 0.5f;
    const std::array images{ std::tuple{ "reset", pageRow(2).x, 2, false },
        std::tuple{ "reset-hover", reset, 2, false },
        std::tuple{ "reset-pressed", reset, 2, true },
        std::tuple{ "reset-disabled", reset, 3, true } };
    for (const auto& [name, x, row, pressed] : images) {
        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = {} });
        WARN("Saved " << renderToFile(panel, name, ImVec2(x, pageRow(row).y), pressed));
    }
}

TEST_CASE("FullPanel render: the keys of a mod's hotkeys", "[.render]")
{
    const test_files::TempFolder files;
    files.write("Config/Hotkeys/keybinds.json", R"({ "modName": "Hotkeys", "keybinds": [
        { "id": "one", "desc": "One", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "id": "two", "desc": "Two", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "id": "three", "desc": "Three", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "id": "four", "desc": "Four", "action": { "type": "RunConsoleCommand", "command": "tgm" } }
    ] })");
    files.write("Settings/Keybinds.json", R"({ "version": 1, "keybinds": [
        { "keycode": 116, "modifiers": 0, "modName": "Hotkeys", "id": "one" },
        { "keycode": 96, "modifiers": 7, "modName": "Hotkeys", "id": "two" },
        { "keycode": 260, "modifiers": 2, "modName": "Hotkeys", "id": "three" }
    ] })");
    mcm::McmStore store(files.path);

    model::Mod mod{ .name = "Hotkeys", .displayName = "Hotkeys", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(model::Hotkey{ .id = "one", .text = "A hotkey with a key", .help = "What the hotkey does." });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "two", .text = "A hotkey with a key and every key that is held with it", .help = "What the hotkey does." });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "three", .text = "A hotkey with a button of the mouse", .help = "What the hotkey does." });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "four", .text = "A hotkey with no key", .help = "What the hotkey does." });
    mod.frontPage.items.push_back(model::Hotkey{ .id = "five", .text = "A hotkey the mod's keybinds.json does not have", .help = "What the hotkey does." });

    // with the ray on a hotkey with a key, and on the one that is not available
    for (const bool available : { true, false }) {
        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = {} });
        WARN("Saved " << renderToFile(panel, available ? "hotkeys" : "hotkeys-not-available", pageRow(available ? 0 : 4)));
    }
}

TEST_CASE("FullPanel render: the texts of a mod's text inputs, and the controls the menu does not have", "[.render]")
{
    using Type = model::StoredType;
    const auto textInput = [](std::string text, std::string id, const Type type) {
        return model::Setting{ .type = model::SettingType::TextInput,
            .text = std::move(text),
            .help = "What the text is.",
            .valueOptions = {},
            .storage = model::ModSettingStorage{ .name = std::move(id), .type = type },
            .valueType = type };
    };
    const test_files::TempFolder files;
    files.write("Config/Texts/settings.ini", "[Main]\nsName=Sanctuary Hills\nsEmpty=\nsLong=A text that is much longer than the box it is shown in is wide\nfNumber=3.14\n");
    mcm::McmStore store(files.path);

    model::Mod mod{ .name = "Texts", .displayName = "Texts", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(textInput("A text", "sName:Main", Type::String));
    mod.frontPage.items.push_back(textInput("An empty text", "sEmpty:Main", Type::String));
    mod.frontPage.items.push_back(textInput("A long text", "sLong:Main", Type::String));
    mod.frontPage.items.push_back(textInput("A number", "fNumber:Main", Type::Float));
    mod.frontPage.items.push_back(textInput("A text that is in no file", "sMissing:Main", Type::String));
    mod.frontPage.items.push_back(model::Placeholder{ .text = "A control the menu does not have", .help = {}, .type = "color" });
    mod.frontPage.items.push_back(model::Placeholder{ .text = "A row with no type", .help = {}, .type = {} });

    // with the ray on a text input, and on a control the menu does not have
    for (const bool onText : { true, false }) {
        FullPanel panel;
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = {} });
        WARN("Saved " << renderToFile(panel, onText ? "text-inputs" : "controls-not-supported", pageRow(onText ? 0 : 5)));
    }

    // the long text is edited, after a press on its row, with more typed at its end, while SteamVR's keyboard
    // holds the ray
    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ mod });
    panel.select({ .mod = 0, .page = {} });
    const auto edit = [&panel] {
        for (const bool held : { false, false, true, false, false }) {
            test_panel::drawFrame(panel, pageRow(2), held);
        }
        ImGui::GetIO().AddInputCharactersUTF8(", and more");
    };
    WARN("Saved " << renderToFile(panel, "text-input-edited", POINTER_AWAY, false, edit));
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
    mcm::McmStore store;

    model::Mod mod{ .name = "Sliders", .displayName = "Sliders", .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(slider("Strength", 0x800, 2.0f, 0.1f));
    mod.frontPage.items.push_back(slider("Brightness", 0x801, 100.0f, 5.0f));
    mod.frontPage.items.push_back(slider("With no range", 0x801, 0.0f, 1.0f));

    FullPanel panel;
    panel.setStore(model::Source::Mcm, &store);
    panel.setMods({ std::move(mod) });
    panel.select({ .mod = 0, .page = {} });
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
    mcm::McmStore store;

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
        panel.setStore(model::Source::Mcm, &store);
        panel.setMods({ mod });
        panel.select({ .mod = 0, .page = {} });
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
        select(panel, "Sim Settlements 2", { "Gameplay" });
        WARN("Saved " << renderToFile(panel, pressed ? "page-row-pressed" : "page-row-hover", pageRow(2), pressed));
    }
}

TEST_CASE("FullPanel render: names too long for the title bar and for the tree", "[.render]")
{
    FullPanel panel;
    panel.setMods({ modOf("Sim Settlements 2 - All Chapters with the City Plan Contest Packs", { "Settlement Gameplay and City Plan Options of Chapter 2" }) });
    select(panel, "Sim Settlements 2 - All Chapters with the City Plan Contest Packs", { "Settlement Gameplay and City Plan Options of Chapter 2" });
    WARN("Saved " << renderToFile(panel, "long-names", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the pages between the mod and the page as one ellipsis, as their crumbs do not fit", "[.render]")
{
    auto mod = modOf("Virtual Holsters", { "Calibration of the whole body" });
    mod.pages[0].pages.push_back({ .name = "Where the hands are held", .items = {} });
    mod.pages[0].pages[0].pages.push_back({ .name = "Fingers of the off hand", .items = {} });
    FullPanel panel;
    panel.setMods({ std::move(mod) });
    panel.select({ .mod = 0, .page = { 0, 0, 0 } });
    WARN("Saved " << renderToFile(panel, "crumbs-elided", POINTER_AWAY));
}

TEST_CASE("FullPanel render: a wider tree and a page in the rest of the panel", "[.render]")
{
    FullPanel panel;
    panel.setMods({ modOf("Sim Settlements 2 - All Chapters with the City Plan Contest Packs", { "Settlement Gameplay and City Plan Options of Chapter 2" }) });
    select(panel, "Sim Settlements 2 - All Chapters with the City Plan Contest Packs", { "Settlement Gameplay and City Plan Options of Chapter 2" });
    panel.setTreeWidth(theme::NAV_WIDTH * 1.5f);
    WARN("Saved " << renderToFile(panel, "wide-tree", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the front page's Developer section, open with the ray on a config spec, folded, and listing all", "[.render]")
{
    const auto configSpec = [](const std::string& name, const int errors, const int warnings, const std::string& notAvailable = {}) {
        model::Mod mod{ .name = name, .displayName = name, .notAvailable = notAvailable, .source = model::Source::ConfigSpec };
        for (int i = 0; i < errors; ++i) {
            mod.problems.push_back({ .severity = model::Severity::Error, .row = "spec.json", .message = "It cannot be read: line 41, column 7.", .where = {}, .path = {} });
        }
        for (int i = 0; i < warnings; ++i) {
            mod.problems.push_back({ .severity = model::Severity::Warning, .row = "A row", .message = "Unknown field.", .where = {}, .path = {} });
        }
        return mod;
    };
    const auto panelWith = [&configSpec](const ui::DeveloperView& view) {
        std::vector<model::Mod> mods;
        mods.push_back(modOf("Baka Fullscreen Pip-Boy"));
        mods.push_back(configSpec("Immersive Flashlight VR", 2, 5));
        mods.push_back(configSpec("Mods Config VR", 0, 0));
        mods.push_back(configSpec("Showcase", 0, 3));
        mods.push_back(configSpec("Virtual Holsters", 1, 0, "its spec.json cannot be read"));
        mods.push_back(configSpec("Sim Settlements 2", 0, 0, "its plugin SS2.esm is not loaded"));
        FullPanel panel;
        panel.setMods(std::move(mods));
        panel.setDeveloper(view, {});
        return panel;
    };

    auto open = panelWith({ .enabled = true, .show = ui::ShowConfigSpecs::NeedingAttention, .configSpec = "Showcase", .folded = false });
    // the ray on the first config spec's row, under the page's text, its notices, the heading and the filter
    WARN("Saved " << renderToFile(open, "front-page-developer", ImVec2(700.0f, 425.0f)));
    auto folded = panelWith({ .enabled = true, .show = ui::ShowConfigSpecs::NeedingAttention, .configSpec = {}, .folded = true });
    WARN("Saved " << renderToFile(folded, "front-page-developer-folded", POINTER_AWAY));
    auto all = panelWith({ .enabled = true, .show = ui::ShowConfigSpecs::All, .configSpec = {}, .folded = false });
    WARN("Saved " << renderToFile(all, "front-page-developer-all", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the config spec showcase's report, with the ray on a problem, and the row a problem shows in its focus ring", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods(mods);
    panel.setDeveloper({ .enabled = true, .show = ui::ShowConfigSpecs::All, .configSpec = {}, .folded = false }, {});
    REQUIRE(panel.openReport(0));
    // the ray on the first error, under the page's text, the live copy and the heading of the errors
    WARN("Saved " << renderToFile(panel, "config-spec-report", ImVec2(900.0f, 575.0f)));

    // the first problem with a row on the Mistakes page: its row in the ring, in a section that was folded or not
    const auto& problems = panel.mods()[0].problems;
    const auto onRow = std::ranges::find_if(problems, [](const model::Problem& problem) {
        return problem.place && problem.where.find("Mistakes") != std::string::npos && problem.message.starts_with("Its applies");
    });
    REQUIRE(onRow != problems.end());
    REQUIRE(panel.showProblem(0, static_cast<std::size_t>(onRow - problems.begin())));
    WARN("Saved " << renderToFile(panel, "config-spec-report-focused-row", POINTER_AWAY));
}

TEST_CASE("FullPanel render: the config spec showcase read from its live copy: its report with Reload's result, a copy that cannot be read, and its Developer row", "[.render]")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    const auto folder = mods[0].folder;
    const auto copy = std::format(R"(My Games\Fallout4VR\Mods_Config\ModsConfigVR\Dev\{}\spec.json)", folder);

    // a panel of its own for each image, as its style is set on the ImGui of its first draw: the showcase read
    // from its live copy, and the copy that cannot be read after it
    const auto panelWith = [&](const bool notRead) {
        FullPanel panel;
        panel.setStore(model::Source::ConfigSpec, &store);
        panel.setMods(mods);
        panel.setDeveloper({ .enabled = true, .show = ui::ShowConfigSpecs::NeedingAttention, .configSpec = {}, .folded = false }, {});
        auto live = mods[0];
        live.liveCopy = copy;
        panel.replaceMod(std::move(live), "10:42:05");
        if (notRead) {
            panel.liveCopyNotRead(folder,
                copy,
                "10:43:12",
                "Not valid JSON: [json.exception.parse_error.101] parse error at line 41, column 7: syntax error while parsing object separator - unexpected string "
                "literal; expected '}'");
        }
        // and its installed spec.json is newer than the copy, which Overwrite warns of, as the menu tells it after a read
        panel.setLiveCopyStale(folder, true);
        return panel;
    };

    // the ray on the heading of the sub-section of the copy, which is folded as the report opens and says the copy is stale
    constexpr ImVec2 LIVE_SPEC_HEADING(900.0f, 293.0f);
    auto folded = panelWith(false);
    REQUIRE(folded.openReport(0));
    WARN("Saved " << renderToFile(folded, "config-spec-report-live-copy-folded", LIVE_SPEC_HEADING));

    // opened by a press on its heading, the ray on Overwrite, which warns, under Reload with its info mark
    auto read = panelWith(false);
    REQUIRE(read.openReport(0));
    const auto open = [&read, LIVE_SPEC_HEADING] {
        for (const bool pressed : { false, false, true, false }) {
            test_panel::drawFrame(read, LIVE_SPEC_HEADING, pressed);
        }
    };
    WARN("Saved " << renderToFile(read, "config-spec-report-live-copy", ImVec2(900.0f, 398.0f), false, open));

    auto notRead = panelWith(true);
    REQUIRE(notRead.openReport(0));
    // the ray on the error of the copy, the first under the heading of the errors, which are scrolled into view
    const auto scrollToErrors = [&notRead] {
        test_panel::drawFrame(notRead, ImVec2(900.0f, 400.0f));
        test_panel::drawFrame(notRead, ImVec2(900.0f, 400.0f));
        ImGui::GetIO().AddMouseWheelEvent(0.0f, -2.0f);
        test_panel::drawFrame(notRead, ImVec2(900.0f, 400.0f));
    };
    WARN("Saved " << renderToFile(notRead, "config-spec-report-live-copy-not-read", ImVec2(900.0f, 400.0f), false, scrollToErrors));

    auto developer = panelWith(false);
    WARN("Saved " << renderToFile(developer, "front-page-developer-live-copy", ImVec2(900.0f, 270.0f)));
}

TEST_CASE("FullPanel render: a button's confirmation, a dangerous one, and the results of its runs", "[.render]")
{
    const test_files::TempFolder myGames;
    spec::ConfigSpecStore store({}, myGames.path);
    auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "FRIK", "applies": "save",
        "content": [
            { "type": "button", "text": "Calibrate height", "help": "Measures your height and updates the calibration.", "buttonText": "Calibrate",
              "action": { "type": "CallNativeFunction", "function": "calibrate" },
              "confirmation": { "title": "Calibrate your height?", "message": "Stand straight and look ahead until it is done.", "confirm": "Start" } },
            { "type": "button", "text": "Reset the calibration", "help": "Puts the body back as it was measured first.", "buttonText": "Reset",
              "action": { "type": "CallNativeFunction", "function": "reset" },
              "confirmation": { "title": "Reset the calibration?", "message": "This discards the current body calibration.", "dangerous": true } },
            { "type": "button", "text": "Open the Pip-Boy config", "help": "Moves the Pip-Boy on your wrist, in the world.", "buttonText": "Open",
              "action": { "type": "CallNativeFunction", "function": "pipboy" } },
            { "type": "button", "text": "Not in this DLL", "action": { "type": "CallNativeFunction", "function": "missing" } }
        ]
    })");
    REQUIRE(mod);
    store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .functions = { { "calibrate",
                                                     [](const auto&) {
                                                         return model::FunctionResult{ .done = true, .message = "Done: 1.78 m." };
                                                     } },
                                      { "reset",
                                          [](const auto&) {
                                              return model::FunctionResult{ .done = true, .message = {} };
                                          } },
                                      { "pipboy",
                                          [](const auto&) {
                                              return model::FunctionResult{ .done = false, .message = "The body is not ready yet." };
                                          } } } }
            .calls());
    FullPanel panel;
    panel.setStore(model::Source::ConfigSpec, &store);
    panel.setMods({ std::move(*mod) });
    panel.select({ .mod = 0, .page = {} });

    const auto press = [&panel](const ImVec2 at) {
        return [&panel, at] {
            for (const bool pressed : { false, false, true, false, false }) {
                test_panel::drawFrame(panel, at, pressed);
            }
        };
    };
    WARN("Saved " << renderToFile(panel, "button-confirmation", POINTER_AWAY, false, press(pageRow(0))));
    panel.back();
    WARN("Saved " << renderToFile(panel, "button-confirmation-dangerous", POINTER_AWAY, false, press(pageRow(1))));
    panel.back();

    // the result of a run that is done, read while the ray is on its row, one that failed, and one that does not run
    WARN("Saved " << renderToFile(panel, "button-result", pageRow(0), false, [&press] {
        press(pageRow(0))();
        const auto* dialog = ImGui::FindWindowByName("##confirmation");
        REQUIRE(dialog);
        // its confirming button, the rightmost
        const ImVec2 confirm(dialog->Pos.x + dialog->Size.x - theme::SPACE_6 - theme::SPACE_4, dialog->Pos.y + dialog->Size.y - theme::SPACE_6 - theme::CONTROL_HEIGHT * 0.5f);
        press(confirm)();
        press(pageRow(2))();
    }));
    WARN("Saved " << renderToFile(panel, "button-failed", pageRow(2), false, press(pageRow(2))));
    WARN("Saved " << renderToFile(panel, "button-not-in-dll", pageRow(3)));
}
