#include <catch2/catch_test_macros.hpp>

#include "TestSwf.h"
#include "mcm/SwfImages.h"

using namespace mods_config;
using test_swf::Pixel;
using test_swf::SwfBuilder;

namespace
{
    constexpr Pixel RED{ 255, 0, 0, 255 };
    constexpr Pixel GREEN{ 0, 255, 0, 255 };
    constexpr Pixel BLUE{ 0, 0, 255, 255 };
    constexpr Pixel HALF_WHITE{ 255, 255, 255, 128 };
    constexpr Pixel CLEAR{ 0, 0, 0, 0 };

    /**
     * A file that has to be read.
     */
    mcm::SwfFile readFile(const test_swf::Bytes& bytes)
    {
        auto swf = mcm::SwfFile::read(bytes);
        if (!swf) {
            FAIL(swf.error());
        }
        return std::move(*swf);
    }

    /**
     * An image of a file that has to be read.
     */
    model::Image imageOf(const SwfBuilder& builder, const std::string_view className, const bool compressed = true)
    {
        auto image = readFile(builder.build(compressed)).image(className);
        if (!image) {
            FAIL(image.error());
        }
        return std::move(*image);
    }

    /**
     * Why a class of a file has no image.
     */
    std::string errorOf(const SwfBuilder& builder, const std::string_view className)
    {
        const auto image = readFile(builder.build()).image(className);
        return image ? "an image" : image.error();
    }

    std::vector<std::uint8_t> rgbaOf(const std::vector<Pixel>& pixels)
    {
        std::vector<std::uint8_t> rgba;
        for (const auto& pixel : pixels) {
            rgba.insert(rgba.end(), { pixel.red, pixel.green, pixel.blue, pixel.alpha });
        }
        return rgba;
    }
}

TEST_CASE("SwfImages: a class that is a bitmap is its pixels, at its size")
{
    SwfBuilder swf;
    swf.bitmap(1, 3, 2, { RED, GREEN, BLUE, HALF_WHITE, CLEAR, RED });
    swf.symbol(1, "Logo");

    const auto image = imageOf(swf, "Logo");

    REQUIRE(image.pixels);
    CHECK(image.pixels->name == "Logo");
    CHECK(image.pixels->width == 3);
    CHECK(image.pixels->height == 2);
    // a color is stored multiplied by its alpha, and is read as the color itself
    CHECK(image.pixels->rgba == rgbaOf({ RED, GREEN, BLUE, HALF_WHITE, CLEAR, RED }));
    CHECK(image.width == 3.0f);
    CHECK(image.height == 2.0f);
}

TEST_CASE("SwfImages: a file that is not compressed is read as one that is")
{
    SwfBuilder swf;
    swf.bitmap(1, 2, 1, { RED, BLUE });
    swf.symbol(1, "Logo");

    CHECK(imageOf(swf, "Logo", false).pixels->rgba == rgbaOf({ RED, BLUE }));
}

TEST_CASE("SwfImages: a bitmap with a table of colors has the color of each pixel")
{
    SwfBuilder swf;
    // five pixels a row, so a row is filled to eight bytes
    swf.colorTableBitmap(1, 5, 2, { RED, GREEN, BLUE }, { 0, 1, 2, 1, 0, 2, 2, 0, 0, 1 });
    swf.symbol(1, "Logo");

    const auto image = imageOf(swf, "Logo");

    CHECK(image.pixels->width == 5);
    CHECK(image.pixels->height == 2);
    CHECK(image.pixels->rgba == rgbaOf({ RED, GREEN, BLUE, GREEN, RED, BLUE, BLUE, RED, RED, GREEN }));
}

TEST_CASE("SwfImages: a sprite that places a shape with a bitmap is the bitmap, at the size of the shape")
{
    SwfBuilder swf;
    swf.bitmap(1, 2, 2, { RED, GREEN, BLUE, RED });
    swf.bitmapShape(2, 1, 180, 183);
    swf.sprite(3, { 2 });
    swf.symbol(3, "Logo");

    const auto image = imageOf(swf, "Logo");

    CHECK(image.pixels->width == 2);
    CHECK(image.pixels->rgba == rgbaOf({ RED, GREEN, BLUE, RED }));
    CHECK(image.width == 180.0f);
    CHECK(image.height == 183.0f);
}

TEST_CASE("SwfImages: a sprite that scales its shape shows it at that size")
{
    SwfBuilder swf;
    swf.bitmap(1, 1, 1, { RED });
    swf.bitmapShape(2, 1, 100, 40);
    swf.sprite(3, { 2 }, 0.5f);
    swf.symbol(3, "Logo");

    const auto image = imageOf(swf, "Logo");

    CHECK(image.width == 50.0f);
    CHECK(image.height == 20.0f);
}

TEST_CASE("SwfImages: a class of code has its bitmap in a class that is named after it")
{
    SwfBuilder swf;
    swf.bitmap(1, 1, 1, { RED });
    swf.bitmap(2, 1, 1, { BLUE });
    swf.symbol(1, "M8r.View.Images.Fonts_bitmap");
    swf.symbol(2, "M8r.View.Intro_bitmap");
    // the class of the file itself shows nothing the file defines
    swf.symbol(0, "M8r.View.Intro");

    // the class of the first is not among the file's classes
    CHECK(imageOf(swf, "M8r.View.Images.Fonts").pixels->rgba == rgbaOf({ RED }));
    CHECK(imageOf(swf, "M8r.View.Intro").pixels->rgba == rgbaOf({ BLUE }));
    CHECK(errorOf(swf, "M8r.View.Images") == "No such class in the file");
}

TEST_CASE("SwfImages: an image that is not a stored bitmap says what it is")
{
    SwfBuilder swf;
    swf.bitmap(1, 1, 1, { RED });
    swf.bitmapShape(2, 1, 10, 10);
    swf.colorShape(3, 10, 10);
    swf.sprite(4, { 3 });
    swf.sprite(5, { 2 }, 1.0f, 12);
    swf.sprite(6, { 2, 3 });
    swf.tag(test_swf::TAG_DEFINE_BITS_JPEG2, { 7, 0, 0xFF, 0xD8, 0xFF, 0xD9 });
    swf.symbol(4, "Vectors");
    swf.symbol(5, "Animated");
    swf.symbol(6, "Drawing");
    swf.symbol(7, "Photo");

    CHECK(errorOf(swf, "Vectors") == "An image that is drawn as vectors, which is not read");
    CHECK(errorOf(swf, "Animated") == "An animated image, which is not read");
    CHECK(errorOf(swf, "Drawing") == "A sprite of 2 parts, which is not read");
    CHECK(errorOf(swf, "Photo") == "A JPEG image, which is not read");
    CHECK(errorOf(swf, "NoSuchClass") == "No such class in the file");
}

TEST_CASE("SwfImages: a file that cannot be read says why")
{
    const auto errorOfFile = [](const test_swf::Bytes& bytes) {
        const auto swf = mcm::SwfFile::read(bytes);
        return swf ? "a file" : swf.error();
    };
    SwfBuilder swf;
    swf.bitmap(1, 1, 1, { RED });
    swf.symbol(1, "Logo");

    CHECK(errorOfFile({}) == "Not an SWF file");
    CHECK(errorOfFile({ 'P', 'K', 3, 4, 0, 0, 0, 0, 0, 0 }) == "Not an SWF file");

    auto lzma = swf.build();
    lzma[0] = 'Z';
    CHECK(errorOfFile(lzma) == "Compressed with LZMA, which is not read");

    auto cut = swf.build();
    cut.resize(cut.size() / 2);
    CHECK(errorOfFile(cut) == "The file cannot be decompressed");
}

TEST_CASE("SwfImages: a bitmap whose data is cut is not read, and the others of the file are")
{
    SwfBuilder swf;
    swf.bitmap(1, 1, 1, { RED });
    // a bitmap of 4 by 4 pixels with no data
    swf.tag(test_swf::TAG_DEFINE_BITS_LOSSLESS2, { 2, 0, 5, 4, 0, 4, 0 });
    // and one that is cut before its size
    swf.tag(test_swf::TAG_DEFINE_BITS_LOSSLESS2, { 3, 0, 5 });
    swf.symbol(1, "Good");
    swf.symbol(2, "NoData");
    swf.symbol(3, "Cut");

    CHECK(imageOf(swf, "Good").pixels->rgba == rgbaOf({ RED }));
    CHECK(errorOf(swf, "NoData") == "The bitmap cannot be decompressed");
    CHECK(errorOf(swf, "Cut") == "A bitmap of 0 by 0 pixels is not read");
}
