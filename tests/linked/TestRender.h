#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "model/ConfigDefinition.h"

// ImGui's frames drawn into an image as a renderer draws them, and saved as a .bmp file, for the render tests, which
// are looked at and check nothing.
namespace test_render
{
    using namespace mods_config;

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
    inline bool isInside(const Corner& a, const Corner& b, const std::int64_t x, const std::int64_t y, std::int64_t& side)
    {
        const std::int64_t dx = b.x - a.x;
        const std::int64_t dy = b.y - a.y;
        side = dx * (y - a.y) - dy * (x - a.x);
        return side > 0 || (side == 0 && (dy > 0 || (dy == 0 && dx < 0)));
    }

    /**
     * A pixel of an image of a mod's page, read at the nearest one: its color, and its opacity.
     */
    inline std::array<float, 4> sample(const model::ImagePixels& pixels, const float u, const float v)
    {
        const auto x = static_cast<std::size_t>(std::clamp(static_cast<int>(u * pixels.width), 0, static_cast<int>(pixels.width) - 1));
        const auto y = static_cast<std::size_t>(std::clamp(static_cast<int>(v * pixels.height), 0, static_cast<int>(pixels.height) - 1));
        const auto* pixel = &pixels.rgba[(y * pixels.width + x) * 4];
        return { static_cast<float>(pixel[0]), static_cast<float>(pixel[1]), static_cast<float>(pixel[2]), pixel[3] / 255.0f };
    }

    /**
     * Fills one triangle of ImGui's, with the colors of its corners and the opacity of the font texture.
     * A triangle of an image of a mod's page has the image's colors and opacity, times those of its corners, which
     * tint an icon.
     */
    inline void fillTriangle(Image& image, const FontTexture& font, const model::ImagePixels* pixels, Corner a, Corner b, Corner c, const ImVec4& clip)
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
                    const auto tint = [&](const std::size_t channel) {
                        return mix(a.color[channel], b.color[channel], c.color[channel]) / 255.0f;
                    };
                    image.blend(x, y, { color[0] * tint(0), color[1] * tint(1), color[2] * tint(2) }, color[3] * tint(3));
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
    inline Image rasterize(const ImDrawData& drawData)
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
    inline void saveBmp(const std::filesystem::path& file, const Image& image)
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
     * Saves the last frame ImGui rendered as an image by its name in the render tests' folder of the build. Returns the
     * path of its file.
     */
    inline std::string saveRender(const std::string& name)
    {
        const std::filesystem::path folder = MODS_CONFIG_TEST_OUTPUT_DIR;
        std::filesystem::create_directories(folder);
        const auto file = folder / (name + ".bmp");
        saveBmp(file, rasterize(*ImGui::GetDrawData()));
        return file.string();
    }
}
