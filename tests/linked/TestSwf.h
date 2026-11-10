#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <zlib.h>

// SWF files made in memory for the tests, as a mod's `lib.swf` has its images.
namespace test_swf
{
    using Bytes = std::vector<std::uint8_t>;

    // the tags of an SWF file that a test file has, by their codes
    constexpr int TAG_SHOW_FRAME = 1;
    constexpr int TAG_DEFINE_SHAPE = 2;
    constexpr int TAG_DEFINE_BITS_LOSSLESS = 20;
    constexpr int TAG_DEFINE_BITS_JPEG2 = 21;
    constexpr int TAG_PLACE_OBJECT2 = 26;
    constexpr int TAG_DEFINE_BITS_LOSSLESS2 = 36;
    constexpr int TAG_DEFINE_SPRITE = 39;
    constexpr int TAG_SYMBOL_CLASS = 76;

    /**
     * A pixel of a test bitmap, with the color itself and not multiplied by its alpha.
     */
    struct Pixel
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::uint8_t alpha = 255;
    };

    inline void addU16(Bytes& bytes, const unsigned value)
    {
        bytes.push_back(static_cast<std::uint8_t>(value));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
    }

    inline void addU32(Bytes& bytes, const std::size_t value)
    {
        addU16(bytes, static_cast<unsigned>(value & 0xFFFF));
        addU16(bytes, static_cast<unsigned>(value >> 16 & 0xFFFF));
    }

    inline void add(Bytes& bytes, const Bytes& more)
    {
        bytes.insert(bytes.end(), more.begin(), more.end());
    }

    /**
     * Values that a file stores as bits, from the high bit of a byte, with the last byte filled with zeros.
     */
    struct Bits
    {
        Bytes bytes;
        int used = 0;

        Bits& add(const std::uint32_t value, const int count)
        {
            for (int i = count - 1; i >= 0; --i) {
                if (used == 0) {
                    bytes.push_back(0);
                }
                bytes.back() |= static_cast<std::uint8_t>((value >> i & 1u) << (7 - used));
                used = (used + 1) % 8;
            }
            return *this;
        }
    };

    /**
     * A rectangle from the origin, of a size in pixels.
     */
    inline Bytes rect(const int width, const int height)
    {
        constexpr int COUNT = 20;
        constexpr int TWIPS = 20;
        return Bits().add(COUNT, 5).add(0, COUNT).add(static_cast<std::uint32_t>(width * TWIPS), COUNT).add(0, COUNT).add(static_cast<std::uint32_t>(height * TWIPS), COUNT).bytes;
    }

    /**
     * A matrix that scales by a number, and one that changes nothing for a scale of 1.
     */
    inline Bytes matrix(const float scale = 1.0f)
    {
        constexpr int COUNT = 24;
        Bits bits;
        if (scale == 1.0f) {
            bits.add(0, 1);
        } else {
            const auto fixed = static_cast<std::uint32_t>(scale * 65536.0f);
            bits.add(1, 1).add(COUNT, 5).add(fixed, COUNT).add(fixed, COUNT);
        }
        return bits.add(0, 1).add(0, 5).bytes;
    }

    inline Bytes deflated(const Bytes& data)
    {
        auto size = compressBound(static_cast<uLong>(data.size()));
        Bytes compressed(size);
        compress(compressed.data(), &size, data.data(), static_cast<uLong>(data.size()));
        compressed.resize(size);
        return compressed;
    }

    /**
     * An SWF file that is built a tag at a time:
     *
     *     SwfBuilder swf;
     *     swf.bitmap(1, 2, 1, { RED, BLUE });
     *     swf.symbol(1, "Logo");
     *     const auto bytes = swf.build();
     */
    class SwfBuilder
    {
    public:
        void tag(const int code, const Bytes& body)
        {
            add(_tags, makeTag(code, body));
        }

        /**
         * A bitmap of four bytes a pixel, with its alpha.
         */
        void bitmap(const unsigned id, const int width, const int height, const std::vector<Pixel>& pixels)
        {
            Bytes data;
            for (const auto& pixel : pixels) {
                data.push_back(pixel.alpha);
                data.push_back(static_cast<std::uint8_t>(pixel.red * pixel.alpha / 255));
                data.push_back(static_cast<std::uint8_t>(pixel.green * pixel.alpha / 255));
                data.push_back(static_cast<std::uint8_t>(pixel.blue * pixel.alpha / 255));
            }
            Bytes body;
            addU16(body, id);
            body.push_back(5);
            addU16(body, static_cast<unsigned>(width));
            addU16(body, static_cast<unsigned>(height));
            add(body, deflated(data));
            tag(TAG_DEFINE_BITS_LOSSLESS2, body);
        }

        /**
         * A bitmap of a byte a pixel into a table of colors, with no alpha. Each row is filled to four bytes.
         */
        void colorTableBitmap(const unsigned id, const int width, const int height, const std::vector<Pixel>& colors, const std::vector<std::uint8_t>& pixels)
        {
            Bytes data;
            for (const auto& color : colors) {
                data.push_back(color.red);
                data.push_back(color.green);
                data.push_back(color.blue);
            }
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < (width + 3) / 4 * 4; ++x) {
                    data.push_back(x < width ? pixels[static_cast<std::size_t>(y) * width + x] : 0);
                }
            }
            Bytes body;
            addU16(body, id);
            body.push_back(3);
            addU16(body, static_cast<unsigned>(width));
            addU16(body, static_cast<unsigned>(height));
            body.push_back(static_cast<std::uint8_t>(colors.size() - 1));
            add(body, deflated(data));
            tag(TAG_DEFINE_BITS_LOSSLESS, body);
        }

        /**
         * A shape of a size in pixels that is filled with a bitmap, as a Flash editor stores a bitmap that is
         * put on the stage: after a fill that names no bitmap.
         */
        void bitmapShape(const unsigned id, const unsigned bitmap, const int width, const int height)
        {
            constexpr std::uint8_t FILL_BITMAP = 0x41;
            Bytes body;
            addU16(body, id);
            add(body, rect(width, height));
            body.push_back(2);
            body.push_back(FILL_BITMAP);
            addU16(body, 0xFFFF);
            add(body, matrix());
            body.push_back(FILL_BITMAP);
            addU16(body, bitmap);
            add(body, matrix());
            // no line styles, and no edges
            add(body, { 0x00, 0x10, 0x00 });
            tag(TAG_DEFINE_SHAPE, body);
        }

        /**
         * A shape that is filled with a color.
         */
        void colorShape(const unsigned id, const int width, const int height)
        {
            Bytes body;
            addU16(body, id);
            add(body, rect(width, height));
            add(body, { 0x01, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x10, 0x00 });
            tag(TAG_DEFINE_SHAPE, body);
        }

        /**
         * A sprite of a number of frames that places the given things, each scaled.
         */
        void sprite(const unsigned id, const std::vector<unsigned>& placed, const float scale = 1.0f, const int frames = 1)
        {
            constexpr std::uint8_t HAS_CHARACTER_AND_MATRIX = 0x06;
            Bytes body;
            addU16(body, id);
            addU16(body, static_cast<unsigned>(frames));
            unsigned depth = 1;
            for (const auto character : placed) {
                Bytes place{ HAS_CHARACTER_AND_MATRIX };
                addU16(place, depth++);
                addU16(place, character);
                add(place, matrix(scale));
                add(body, makeTag(TAG_PLACE_OBJECT2, place));
            }
            add(body, makeTag(TAG_SHOW_FRAME, {}));
            add(body, makeTag(0, {}));
            tag(TAG_DEFINE_SPRITE, body);
        }

        /**
         * A class of the file, which shows what the file defines under a number.
         */
        void symbol(const unsigned id, const std::string_view name)
        {
            _symbols.emplace_back(id, name);
        }

        /**
         * The file: its header, its tags and then its classes, compressed with zlib or not.
         */
        Bytes build(const bool compressed = true) const
        {
            Bytes data = rect(550, 400);
            addU16(data, 24 << 8);
            addU16(data, 1);
            add(data, _tags);
            if (!_symbols.empty()) {
                Bytes body;
                addU16(body, static_cast<unsigned>(_symbols.size()));
                for (const auto& [id, name] : _symbols) {
                    addU16(body, id);
                    body.insert(body.end(), name.begin(), name.end());
                    body.push_back(0);
                }
                add(data, makeTag(TAG_SYMBOL_CLASS, body));
            }
            add(data, makeTag(TAG_SHOW_FRAME, {}));
            add(data, makeTag(0, {}));

            Bytes file{ static_cast<std::uint8_t>(compressed ? 'C' : 'F'), 'W', 'S', 10 };
            addU32(file, data.size() + 8);
            add(file, compressed ? deflated(data) : data);
            return file;
        }

    private:
        /**
         * A tag in its long form, which a tag of any length can have.
         */
        static Bytes makeTag(const int code, const Bytes& body)
        {
            Bytes tag;
            addU16(tag, static_cast<unsigned>(code << 6 | 0x3F));
            addU32(tag, body.size());
            add(tag, body);
            return tag;
        }

        Bytes _tags;
        std::vector<std::pair<unsigned, std::string>> _symbols;
    };
}
