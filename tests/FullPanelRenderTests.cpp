#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>

#include <imgui.h>

#include "Theme.h"
#include "imgui/ImGuiFonts.h"
#include "ui/FullPanel.h"

// The Full panel drawn without the game and saved as an image, to look at a change of the UI before it is tried
// in the headset. They check nothing, so they are not run with the other tests. Run them by their tag:
//     ModsConfigVR_tests "[render]"
// Each says where its image is, a .bmp file in the build folder.

using namespace mods_config;
using ui::FullPanel;

namespace
{
    // the panel as ConfigMenu.cpp makes it, in the panel's pixels, and what the framework draws it with
    constexpr float PANEL_WIDTH = theme::PANEL_WIDTH;
    constexpr float PANEL_HEIGHT = theme::PANEL_HEIGHT;
    constexpr float PANEL_BORDER = theme::BORDER_PANEL;
    constexpr float PANEL_RADIUS = theme::RADIUS_PANEL;
    constexpr float PANEL_PADDING = theme::PANEL_PADDING;
    constexpr ImU32 PANEL_BACKGROUND = IM_COL32(15, 15, 15, 166);
    constexpr float FONT_SIZE = 24.0f;
    constexpr float SUPERSAMPLE = 1.5f;

    // the world behind the panel's glass
    constexpr std::array<std::uint8_t, 3> WORLD{ 58, 60, 58 };

    // a pointer that is on nothing
    constexpr ImVec2 POINTER_AWAY(-1000.0f, -1000.0f);

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
     * Fills one triangle of ImGui's, with the colors of its corners and the opacity of the font texture.
     */
    void fillTriangle(Image& image, const FontTexture& font, Corner a, Corner b, Corner c, const ImVec4& clip)
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
                const float opacity = mix(a.color[3], b.color[3], c.color[3]) / 255.0f * font.sample(mix(a.uv.x, b.uv.x, c.uv.x), mix(a.uv.y, b.uv.y, c.uv.y));
                image.blend(x, y, { mix(a.color[0], b.color[0], c.color[0]), mix(a.color[1], b.color[1], c.color[1]), mix(a.color[2], b.color[2], c.color[2]) }, opacity);
            }
        }
    }

    /**
     * Draws a frame of ImGui's into an image, as a renderer does: every triangle of every draw command,
     * inside the command's clip rectangle.
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
                for (unsigned int i = 0; i + 2 < command.ElemCount; i += 3) {
                    fillTriangle(image, font, Corner(vertexes[indexes[i]], scale), Corner(vertexes[indexes[i + 1]], scale), Corner(vertexes[indexes[i + 2]], scale), clip);
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
     * An ImGui context set up as the framework sets its own up for the menu's panel, with no renderer.
     */
    struct PanelContext
    {
        FontTexture font;

        PanelContext()
        {
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.ConfigInputTrickleEventQueue = false;
            io.DisplaySize = ImVec2(PANEL_WIDTH, PANEL_HEIGHT);
            io.FontGlobalScale = 1.0f / SUPERSAMPLE;
            ImGui::StyleColorsDark();
            f4cf::imgui::internal::loadCanvasFont(FONT_SIZE, SUPERSAMPLE);
            io.Fonts->GetTexDataAsAlpha8(&font.alpha, &font.width, &font.height);
        }

        PanelContext(const PanelContext&) = delete;
        PanelContext& operator=(const PanelContext&) = delete;

        ~PanelContext()
        {
            ImGui::DestroyContext();
        }
    };

    /**
     * One frame of the panel, in a window as the framework's canvas makes it: the panel's background and its
     * border, and the content inside its padding. The pointer is where the ray is on the panel.
     */
    void drawFrame(FullPanel& panel, const ImVec2 pointer)
    {
        constexpr auto WINDOW_FLAGS = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground |
                                      ImGuiWindowFlags_NoScrollbar;
        constexpr float HALF_BORDER = PANEL_BORDER * 0.5f;
        constexpr float FRINGE_SCALE = 1.0f / SUPERSAMPLE;

        auto& io = ImGui::GetIO();
        io.DeltaTime = 1.0f / 90.0f;
        io.AddMousePosEvent(pointer.x, pointer.y);
        ImGui::NewFrame();

        const ImVec2 windowMin(HALF_BORDER, HALF_BORDER);
        const ImVec2 windowMax(PANEL_WIDTH - HALF_BORDER, PANEL_HEIGHT - HALF_BORDER);
        ImGui::SetNextWindowPos(windowMin);
        ImGui::SetNextWindowSize(ImVec2(PANEL_WIDTH - PANEL_BORDER, PANEL_HEIGHT - PANEL_BORDER));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Panel", nullptr, WINDOW_FLAGS);
        ImGui::PopStyleVar(2);

        auto* drawList = ImGui::GetWindowDrawList();
        drawList->_FringeScale = FRINGE_SCALE;
        drawList->AddRectFilled(windowMin, windowMax, PANEL_BACKGROUND, PANEL_RADIUS - HALF_BORDER);
        drawList->AddRect(windowMin, windowMax, theme::color::GREEN, PANEL_RADIUS - HALF_BORDER, ImDrawFlags_None, PANEL_BORDER);

        ImGui::SetCursorPos(ImVec2(PANEL_PADDING + HALF_BORDER, PANEL_PADDING + HALF_BORDER));
        const ImVec2 contentSize(PANEL_WIDTH - (PANEL_PADDING + PANEL_BORDER) * 2.0f, PANEL_HEIGHT - (PANEL_PADDING + PANEL_BORDER) * 2.0f);
        if (ImGui::BeginChild("content", contentSize, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
            ImGui::GetWindowDrawList()->_FringeScale = FRINGE_SCALE;
            ImGui::BeginGroup();
            panel.draw();
            ImGui::EndGroup();
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::Render();
    }

    /**
     * Draws the panel with the pointer at a place and saves it as an image, at the size the framework
     * rasterizes the panel. A few frames are drawn first: ImGui needs them to find what the pointer is on.
     */
    std::string renderToFile(FullPanel& panel, const std::string& name, const ImVec2 pointer)
    {
        const PanelContext context;
        for (int i = 0; i < 3; ++i) {
            drawFrame(panel, pointer);
        }
        const std::filesystem::path folder = MODS_CONFIG_TEST_OUTPUT_DIR;
        std::filesystem::create_directories(folder);
        const auto file = folder / (name + ".bmp");
        saveBmp(file, rasterize(*ImGui::GetDrawData(), context.font, SUPERSAMPLE));
        return file.string();
    }

    /**
     * The panel with the mods of the test set, and FRIK in its quick launch section.
     */
    FullPanel testSetPanel()
    {
        std::vector<model::Mod> mods;
        for (const auto* name : { "XDI",
                 "Sim Settlements 2",
                 "Auto Eat and Drink in Survival",
                 "FallUI - Workbench",
                 "Chemfluence",
                 "NPCs Use Items",
                 "Workshop Framework",
                 "FallUI - Inventory",
                 "Unlimited Survival Mode" }) {
            mods.push_back({ .name = name, .displayName = name, .frontPage = {}, .pages = {} });
        }
        FullPanel panel;
        panel.setMods(std::move(mods));
        panel.setQuickLaunch({ { .name = "FRIK", .open = [] {
                                } } });
        return panel;
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
