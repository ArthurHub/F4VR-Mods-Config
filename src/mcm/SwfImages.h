#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "model/Definition.h"

namespace mods_config::mcm
{
    /**
     * A mod's `lib.swf`, which holds the images of its MCM menu. An image row names a class of the file, and
     * MCM's own menu makes an object of that class with Flash. This reads the bitmap the class shows out of the
     * file's tags, and runs no Flash:
     *
     *     if (const auto swf = SwfFile::read(bytes)) {
     *         const auto image = swf->image("Logo");
     *     }
     *
     * A class is found as the two kinds of the MCM mods have it: a sprite that places one shape filled with a
     * bitmap, and a class of code with the bitmap in a class of its own, named `<class>_<member>`.
     * A bitmap is one that is stored without loss. An image that is a JPEG, that is drawn as vectors or that is
     * animated is not read, and the error says which.
     */
    class SwfFile
    {
    public:
        static std::expected<SwfFile, std::string> read(std::span<const std::uint8_t> file);

        std::expected<model::Image, std::string> image(std::string_view className) const;
        std::string mainClass() const;

    private:
        /**
         * A tag of the file: its code, and where its body is in the data.
         */
        struct Tag
        {
            int code = 0;
            std::size_t offset = 0;
            std::size_t length = 0;
        };

        /**
         * The pixels of a bitmap, and the size Flash shows it at, in the file's pixels.
         */
        struct Picture
        {
            std::shared_ptr<model::ImagePixels> pixels;
            float width = 0.0f;
            float height = 0.0f;
        };

        void readTags();
        std::span<const std::uint8_t> bodyOf(const Tag& tag) const;
        std::expected<Picture, std::string> pictureOf(std::uint16_t character, int depth) const;
        std::expected<Picture, std::string> bitmapPicture(const Tag& tag) const;
        std::expected<Picture, std::string> shapePicture(const Tag& tag, int depth) const;
        std::expected<Picture, std::string> spritePicture(const Tag& tag, int depth) const;

        // the file without its first 8 bytes, and not compressed
        std::vector<std::uint8_t> _data;

        // what the file defines, by the number each has in it
        std::unordered_map<std::uint16_t, Tag> _characters;

        // the classes of the file, each with the number of what it shows
        std::vector<std::pair<std::string, std::uint16_t>> _symbols;
    };
}
