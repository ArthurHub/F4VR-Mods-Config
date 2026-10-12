#include "SwfImages.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <optional>

#include <zlib.h>

using namespace mods_config;

namespace
{
    // the tags of an SWF file that are read, by their codes
    constexpr int TAG_END = 0;
    constexpr int TAG_DEFINE_SHAPE = 2;
    constexpr int TAG_PLACE_OBJECT = 4;
    constexpr int TAG_DEFINE_BITS = 6;
    constexpr int TAG_DEFINE_BITS_LOSSLESS = 20;
    constexpr int TAG_DEFINE_BITS_JPEG2 = 21;
    constexpr int TAG_DEFINE_SHAPE2 = 22;
    constexpr int TAG_PLACE_OBJECT2 = 26;
    constexpr int TAG_DEFINE_SHAPE3 = 32;
    constexpr int TAG_DEFINE_BITS_JPEG3 = 35;
    constexpr int TAG_DEFINE_BITS_LOSSLESS2 = 36;
    constexpr int TAG_DEFINE_SPRITE = 39;
    constexpr int TAG_PLACE_OBJECT3 = 70;
    constexpr int TAG_SYMBOL_CLASS = 76;
    constexpr int TAG_DEFINE_SHAPE4 = 83;
    constexpr int TAG_DEFINE_BITS_JPEG4 = 90;

    // the formats of a bitmap that is stored without loss: a byte a pixel into a table of colors, and four bytes a pixel
    constexpr int FORMAT_COLOR_TABLE = 3;
    constexpr int FORMAT_FOUR_BYTES = 5;

    // a fill of a shape: one color, and the four kinds of a bitmap
    constexpr int FILL_SOLID = 0x00;
    constexpr int FILL_BITMAP_FIRST = 0x40;
    constexpr int FILL_BITMAP_LAST = 0x43;

    // the bitmap a Flash editor names in a fill that fills nothing
    constexpr std::uint16_t NO_BITMAP = 0xFFFF;

    // a place or a size in a file is in these parts of a pixel
    constexpr float TWIPS_IN_PIXEL = 20.0f;

    // The most a file and a bitmap are read at, so a file that is wrong asks for no memory it has no use for
    constexpr std::size_t MAX_FILE_SIZE = 256u * 1024 * 1024;
    constexpr std::size_t MAX_BITMAP_PIXELS = 8192u * 8192;

    // a class that shows another, which shows another: more than this deep is a file that is wrong
    constexpr int MAX_DEPTH = 8;

    /**
     * Reads the values of a tag in the order they are stored: whole bytes with the low one first, and bits
     * from the high one of a byte. A read past the end gives 0 and marks the reader as failed.
     */
    class Reader
    {
    public:
        explicit Reader(const std::span<const std::uint8_t> data)
            : _data(data)
        {}

        bool failed() const
        {
            return _failed;
        }

        std::uint8_t u8()
        {
            align();
            if (_pos >= _data.size()) {
                _failed = true;
                return 0;
            }
            return _data[_pos++];
        }

        std::uint16_t u16()
        {
            const std::uint16_t low = u8();
            return static_cast<std::uint16_t>(low | u8() << 8);
        }

        std::uint32_t bits(const int count)
        {
            std::uint32_t value = 0;
            for (int i = 0; i < count; ++i) {
                if (_pos >= _data.size()) {
                    _failed = true;
                    return 0;
                }
                value = value << 1 | ((_data[_pos] >> (7 - _bit)) & 1u);
                if (++_bit == 8) {
                    _bit = 0;
                    ++_pos;
                }
            }
            return value;
        }

        std::int32_t signedBits(const int count)
        {
            const auto value = bits(count);
            const bool negative = count > 0 && count < 32 && (value >> (count - 1)) != 0;
            return static_cast<std::int32_t>(negative ? value | ~0u << count : value);
        }

        /**
         * To the start of the next byte, after values that are read as bits.
         */
        void align()
        {
            if (_bit != 0) {
                _bit = 0;
                ++_pos;
            }
        }

        /**
         * A text that ends in a zero byte.
         */
        std::string text()
        {
            std::string text;
            for (auto letter = u8(); letter != 0 && !_failed; letter = u8()) {
                text.push_back(static_cast<char>(letter));
            }
            return text;
        }

        /**
         * What is left of the data, from the next byte.
         */
        std::span<const std::uint8_t> rest()
        {
            align();
            return _data.subspan((std::min)(_pos, _data.size()));
        }

    private:
        std::span<const std::uint8_t> _data;
        std::size_t _pos = 0;
        int _bit = 0;
        bool _failed = false;
    };

    /**
     * A rectangle of a file, in twips.
     */
    struct Rect
    {
        std::int32_t left = 0;
        std::int32_t right = 0;
        std::int32_t top = 0;
        std::int32_t bottom = 0;
    };

    Rect readRect(Reader& reader)
    {
        const int count = static_cast<int>(reader.bits(5));
        Rect rect;
        rect.left = reader.signedBits(count);
        rect.right = reader.signedBits(count);
        rect.top = reader.signedBits(count);
        rect.bottom = reader.signedBits(count);
        reader.align();
        return rect;
    }

    /**
     * How much a matrix of a file scales what it places, across and down. Its turn and its move are passed over.
     */
    struct Scale
    {
        float x = 1.0f;
        float y = 1.0f;
    };

    Scale readMatrix(Reader& reader)
    {
        constexpr float FIXED_ONE = 65536.0f;
        Scale scale;
        if (reader.bits(1) != 0) {
            const int count = static_cast<int>(reader.bits(5));
            scale.x = std::abs(static_cast<float>(reader.signedBits(count)) / FIXED_ONE);
            scale.y = std::abs(static_cast<float>(reader.signedBits(count)) / FIXED_ONE);
        }
        if (reader.bits(1) != 0) {
            const int count = static_cast<int>(reader.bits(5));
            reader.bits(count);
            reader.bits(count);
        }
        const int count = static_cast<int>(reader.bits(5));
        reader.bits(count);
        reader.bits(count);
        reader.align();
        return scale;
    }

    /**
     * Calls the visitor with the code of every tag of the data, and with where its body starts and how long
     * it is. It ends at the end tag, and at a tag that is longer than the data that is left.
     */
    template <typename Visitor>
    void forEachTag(const std::span<const std::uint8_t> data, std::size_t pos, Visitor&& visit)
    {
        while (pos + 2 <= data.size()) {
            const unsigned header = data[pos] | data[pos + 1] << 8;
            pos += 2;
            const int code = static_cast<int>(header >> 6);
            std::size_t length = header & 0x3F;
            if (length == 0x3F) {
                if (pos + 4 > data.size()) {
                    return;
                }
                length = data[pos] | data[pos + 1] << 8 | data[pos + 2] << 16 | static_cast<std::size_t>(data[pos + 3]) << 24;
                pos += 4;
            }
            if (code == TAG_END || length > data.size() - pos) {
                return;
            }
            visit(code, pos, length);
            pos += length;
        }
    }

    /**
     * Data that zlib compressed, as the given number of bytes it holds. None for data that is not zlib's, and
     * for data that holds less.
     */
    std::optional<std::vector<std::uint8_t>> inflateTo(const std::span<const std::uint8_t> compressed, const std::size_t size)
    {
        std::vector<std::uint8_t> data(size);
        if (size == 0 || compressed.empty()) {
            return size == 0 ? std::optional(std::move(data)) : std::nullopt;
        }
        z_stream stream{};
        if (inflateInit(&stream) != Z_OK) {
            return std::nullopt;
        }
        stream.next_in = const_cast<Bytef*>(compressed.data());
        stream.avail_in = static_cast<uInt>(compressed.size());
        stream.next_out = data.data();
        stream.avail_out = static_cast<uInt>(size);
        const int result = inflate(&stream, Z_FINISH);
        const bool filled = stream.avail_out == 0;
        inflateEnd(&stream);
        // data that holds more than was asked for ends with a full buffer and no end of the stream
        if (!filled || (result != Z_STREAM_END && result != Z_OK && result != Z_BUF_ERROR)) {
            return std::nullopt;
        }
        return data;
    }

    /**
     * Adds a pixel of a bitmap to the pixels of an image. A file stores a color already multiplied by its
     * alpha, and an image has the color itself.
     */
    void addPixel(std::vector<std::uint8_t>& rgba, const std::uint8_t alpha, const std::uint8_t red, const std::uint8_t green, const std::uint8_t blue)
    {
        const auto color = [alpha](const std::uint8_t value) {
            return alpha == 0 || alpha == 255 ? value : static_cast<std::uint8_t>((std::min)(value * 255 / alpha, 255));
        };
        rgba.push_back(color(red));
        rgba.push_back(color(green));
        rgba.push_back(color(blue));
        rgba.push_back(alpha);
    }

    bool isBitmap(const int code)
    {
        return code == TAG_DEFINE_BITS_LOSSLESS || code == TAG_DEFINE_BITS_LOSSLESS2;
    }

    bool isJpeg(const int code)
    {
        return code == TAG_DEFINE_BITS || code == TAG_DEFINE_BITS_JPEG2 || code == TAG_DEFINE_BITS_JPEG3 || code == TAG_DEFINE_BITS_JPEG4;
    }

    bool isShape(const int code)
    {
        return code == TAG_DEFINE_SHAPE || code == TAG_DEFINE_SHAPE2 || code == TAG_DEFINE_SHAPE3 || code == TAG_DEFINE_SHAPE4;
    }
}

namespace mods_config::mcm
{
    /**
     * Reads the bytes of an SWF file: it finds what the file defines and the classes it has, and reads no
     * image yet. A file that zlib compressed is read, and one that is not compressed.
     * The error says why a file cannot be read.
     */
    std::expected<SwfFile, std::string> SwfFile::read(const std::span<const std::uint8_t> file)
    {
        constexpr std::size_t HEADER_SIZE = 8;
        if (file.size() < HEADER_SIZE || file[1] != 'W' || file[2] != 'S' || (file[0] != 'F' && file[0] != 'C' && file[0] != 'Z')) {
            return std::unexpected("Not an SWF file"s);
        }
        if (file[0] == 'Z') {
            return std::unexpected("Compressed with LZMA, which is not read"s);
        }
        const std::size_t length = file[4] | file[5] << 8 | file[6] << 16 | static_cast<std::size_t>(file[7]) << 24;
        if (length < HEADER_SIZE || length > MAX_FILE_SIZE) {
            return std::unexpected(std::format("A file of {} bytes is not read", length));
        }

        SwfFile swf;
        if (file[0] == 'C') {
            auto data = inflateTo(file.subspan(HEADER_SIZE), length - HEADER_SIZE);
            if (!data) {
                return std::unexpected("The file cannot be decompressed"s);
            }
            swf._data = std::move(*data);
        } else {
            swf._data.assign(file.begin() + HEADER_SIZE, file.end());
        }
        swf.readTags();
        return swf;
    }

    /**
     * The image a class of the file shows, at the size Flash shows it.
     * A class that shows nothing by itself is a class of code. Its bitmap is then in a class of its own, which
     * the compiler names after it: `<class>_<member>`.
     * The error says why a class has no image that is read.
     */
    std::expected<model::Image, std::string> SwfFile::image(const std::string_view className) const
    {
        std::expected<Picture, std::string> picture = std::unexpected("No such class in the file"s);
        for (const auto& [name, character] : _symbols) {
            if (name == className) {
                picture = pictureOf(character, 0);
                break;
            }
        }
        if (!picture) {
            const auto prefix = std::string(className) + "_";
            for (const auto& [name, character] : _symbols) {
                if (!name.starts_with(prefix)) {
                    continue;
                }
                if (auto ofMember = pictureOf(character, 0)) {
                    picture = std::move(ofMember);
                    break;
                }
            }
        }
        if (!picture) {
            return std::unexpected(picture.error());
        }
        picture->pixels->name = className;
        return model::Image{ .pixels = std::move(picture->pixels), .width = picture->width, .height = picture->height };
    }

    /**
     * Finds what the file defines, each by its number, and its classes. The tags start after the size of the
     * stage, the frame rate and the number of frames.
     */
    void SwfFile::readTags()
    {
        Reader header(_data);
        readRect(header);
        header.u16();
        header.u16();
        if (header.failed()) {
            return;
        }
        const std::span<const std::uint8_t> data(_data);
        forEachTag(data, _data.size() - header.rest().size(), [this, data](const int code, const std::size_t offset, const std::size_t length) {
            Reader reader(data.subspan(offset, length));
            if (code == TAG_SYMBOL_CLASS) {
                const int count = reader.u16();
                for (int i = 0; i < count && !reader.failed(); ++i) {
                    const auto character = reader.u16();
                    _symbols.emplace_back(reader.text(), character);
                }
            } else if (isBitmap(code) || isJpeg(code) || isShape(code) || code == TAG_DEFINE_SPRITE) {
                const auto character = reader.u16();
                if (!reader.failed()) {
                    _characters[character] = Tag{ .code = code, .offset = offset, .length = length };
                }
            }
        });
    }

    std::span<const std::uint8_t> SwfFile::bodyOf(const Tag& tag) const
    {
        return std::span<const std::uint8_t>(_data).subspan(tag.offset, tag.length);
    }

    /**
     * The picture of what the file defines under a number: a bitmap, a shape that is filled with one, or a
     * sprite that places one of them.
     */
    std::expected<SwfFile::Picture, std::string> SwfFile::pictureOf(const std::uint16_t character, const int depth) const
    {
        const auto found = _characters.find(character);
        if (found == _characters.end() || depth > MAX_DEPTH) {
            return std::unexpected("The class shows nothing that is read"s);
        }
        const auto& tag = found->second;
        if (isBitmap(tag.code)) {
            return bitmapPicture(tag);
        }
        if (isJpeg(tag.code)) {
            return std::unexpected("A JPEG image, which is not read"s);
        }
        if (isShape(tag.code)) {
            return shapePicture(tag, depth);
        }
        return spritePicture(tag, depth);
    }

    /**
     * The pixels of a bitmap that is stored without loss, in one of its two formats: four bytes a pixel, and
     * a byte a pixel into a table of colors, with each row filled to four bytes.
     * Only the tag of the second kind has an alpha.
     */
    std::expected<SwfFile::Picture, std::string> SwfFile::bitmapPicture(const Tag& tag) const
    {
        Reader reader(bodyOf(tag));
        reader.u16();
        const int format = reader.u8();
        const std::size_t width = reader.u16();
        const std::size_t height = reader.u16();
        const bool hasAlpha = tag.code == TAG_DEFINE_BITS_LOSSLESS2;
        if (reader.failed() || width == 0 || height == 0 || width * height > MAX_BITMAP_PIXELS) {
            return std::unexpected(std::format("A bitmap of {} by {} pixels is not read", width, height));
        }

        auto pixels = std::make_shared<model::ImagePixels>();
        pixels->width = static_cast<std::uint32_t>(width);
        pixels->height = static_cast<std::uint32_t>(height);
        pixels->rgba.reserve(width * height * 4);

        if (format == FORMAT_FOUR_BYTES) {
            const auto data = inflateTo(reader.rest(), width * height * 4);
            if (!data) {
                return std::unexpected("The bitmap cannot be decompressed"s);
            }
            for (std::size_t i = 0; i < data->size(); i += 4) {
                const auto* pixel = data->data() + i;
                addPixel(pixels->rgba, hasAlpha ? pixel[0] : 255, pixel[1], pixel[2], pixel[3]);
            }
        } else if (format == FORMAT_COLOR_TABLE) {
            const std::size_t colors = reader.u8() + 1u;
            const std::size_t colorSize = hasAlpha ? 4 : 3;
            const std::size_t rowSize = (width + 3) & ~std::size_t{ 3 };
            const auto data = inflateTo(reader.rest(), colors * colorSize + rowSize * height);
            if (!data) {
                return std::unexpected("The bitmap cannot be decompressed"s);
            }
            const auto* rows = data->data() + colors * colorSize;
            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    // a pixel that names a color the table lacks is the table's first
                    const std::size_t index = rows[y * rowSize + x];
                    const auto* color = data->data() + (index < colors ? index : 0) * colorSize;
                    addPixel(pixels->rgba, hasAlpha ? color[3] : 255, color[0], color[1], color[2]);
                }
            }
        } else {
            return std::unexpected(std::format("A bitmap of format {} is not read", format));
        }
        return Picture{ .pixels = std::move(pixels), .width = static_cast<float>(width), .height = static_cast<float>(height) };
    }

    /**
     * The picture of a shape that is filled with a bitmap: the bitmap, at the size of the shape.
     * A Flash editor stores a bitmap that is put on the stage as such a shape, after a fill that names no
     * bitmap. A shape with a gradient, and one with no bitmap, is drawn as vectors.
     */
    std::expected<SwfFile::Picture, std::string> SwfFile::shapePicture(const Tag& tag, const int depth) const
    {
        Reader reader(bodyOf(tag));
        reader.u16();
        const auto bounds = readRect(reader);
        if (tag.code == TAG_DEFINE_SHAPE4) {
            readRect(reader);
            reader.u8();
        }
        int fills = reader.u8();
        if (fills == 0xFF && tag.code != TAG_DEFINE_SHAPE) {
            fills = reader.u16();
        }
        const bool colorHasAlpha = tag.code == TAG_DEFINE_SHAPE3 || tag.code == TAG_DEFINE_SHAPE4;

        for (int i = 0; i < fills && !reader.failed(); ++i) {
            const int fill = reader.u8();
            if (fill == FILL_SOLID) {
                for (int part = 0; part < (colorHasAlpha ? 4 : 3); ++part) {
                    reader.u8();
                }
                continue;
            }
            if (fill < FILL_BITMAP_FIRST || fill > FILL_BITMAP_LAST) {
                break;
            }
            const auto bitmap = reader.u16();
            readMatrix(reader);
            if (bitmap == NO_BITMAP || reader.failed()) {
                continue;
            }
            const auto found = _characters.find(bitmap);
            if (found == _characters.end() || isShape(found->second.code)) {
                break;
            }
            auto picture = pictureOf(bitmap, depth + 1);
            if (picture) {
                picture->width = static_cast<float>(bounds.right - bounds.left) / TWIPS_IN_PIXEL;
                picture->height = static_cast<float>(bounds.bottom - bounds.top) / TWIPS_IN_PIXEL;
            }
            return picture;
        }
        return std::unexpected("An image that is drawn as vectors, which is not read"s);
    }

    /**
     * The picture of a sprite of one frame that places one thing: that thing, as large as the sprite scales it.
     * A sprite of more frames is animated, and one that places more is a drawing of its own.
     */
    std::expected<SwfFile::Picture, std::string> SwfFile::spritePicture(const Tag& tag, const int depth) const
    {
        constexpr std::size_t TAGS_START = 4;
        constexpr int HAS_CHARACTER = 0x02;
        constexpr int HAS_MATRIX = 0x04;
        constexpr int HAS_CLASS_NAME = 0x08;
        constexpr int HAS_IMAGE = 0x10;

        const auto body = bodyOf(tag);
        Reader header(body);
        header.u16();
        const int frames = header.u16();
        if (header.failed() || frames > 1) {
            return std::unexpected("An animated image, which is not read"s);
        }

        int placed = 0;
        std::uint16_t character = 0;
        Scale scale;
        forEachTag(body, TAGS_START, [&](const int code, const std::size_t offset, const std::size_t length) {
            Reader reader(body.subspan(offset, length));
            int flags = HAS_CHARACTER | HAS_MATRIX;
            if (code == TAG_PLACE_OBJECT) {
                character = reader.u16();
                reader.u16();
            } else if (code == TAG_PLACE_OBJECT2 || code == TAG_PLACE_OBJECT3) {
                flags = reader.u8();
                const int moreFlags = code == TAG_PLACE_OBJECT3 ? reader.u8() : 0;
                reader.u16();
                if ((moreFlags & HAS_CLASS_NAME) != 0 || ((moreFlags & HAS_IMAGE) != 0 && (flags & HAS_CHARACTER) != 0)) {
                    reader.text();
                }
                if ((flags & HAS_CHARACTER) == 0) {
                    return;
                }
                character = reader.u16();
            } else {
                return;
            }
            scale = (flags & HAS_MATRIX) != 0 ? readMatrix(reader) : Scale{};
            ++placed;
        });
        if (placed != 1) {
            return std::unexpected(std::format("A sprite of {} parts, which is not read", placed));
        }

        auto picture = pictureOf(character, depth + 1);
        if (picture) {
            picture->width *= scale.x;
            picture->height *= scale.y;
        }
        return picture;
    }
}
