#include "McmConfigImages.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <vector>

using namespace mods_config;

namespace
{
    // the file of a mod's config folder that holds the images of its menu
    constexpr auto IMAGE_LIBRARY_FILE = "lib.swf";

    // the most an image is wide and high in MCM's menu, in its pixels: a larger one is shown smaller
    constexpr float MAX_IMAGE_WIDTH = 690.0f;
    constexpr float MAX_IMAGE_HEIGHT = 400.0f;
}

namespace mods_config::mcm
{
    McmConfigImages::McmConfigImages(std::filesystem::path configFolder)
        : _configFolder(std::move(configFolder))
    {}

    /**
     * The image of a row, at the size MCM's menu shows it: as large as it is, and smaller where that is more
     * than the menu has room for.
     * An image that cannot be read has no pixels, with the reason in the log.
     */
    model::Image McmConfigImages::load(const std::string& libName, const std::string& className)
    {
        const auto& library = libraryOf(libName);
        auto image = library ? library->image(className) : std::unexpected(library.error());
        if (!image) {
            logger::warn("MCM image '{}' of '{}' is not shown: {}", className, libName, image.error());
            return {};
        }
        const float fit = (std::min)({ 1.0f, MAX_IMAGE_WIDTH / image->width, MAX_IMAGE_HEIGHT / image->height });
        image->width *= fit;
        image->height *= fit;
        return std::move(*image);
    }

    /**
     * The image file of a mod, by the name of the mod's config folder. It is read when it is first asked
     * for, and a file that is not there or is not read is asked for once too.
     */
    const std::expected<SwfFile, std::string>& McmConfigImages::libraryOf(const std::string& libName)
    {
        if (const auto found = _libraries.find(libName); found != _libraries.end()) {
            return found->second;
        }
        // a name of a config is UTF-8, which a path takes as such only from a u8string
        const auto file = _configFolder / std::filesystem::path(std::u8string(libName.begin(), libName.end())) / IMAGE_LIBRARY_FILE;
        std::error_code noFile;
        if (libName.empty() || _configFolder.empty() || !std::filesystem::is_regular_file(file, noFile)) {
            return _libraries.emplace(libName, std::unexpected(std::format("The mod has no {}", IMAGE_LIBRARY_FILE))).first->second;
        }
        std::ifstream stream(file, std::ios::binary);
        const std::vector<char> bytes{ std::istreambuf_iterator(stream), std::istreambuf_iterator<char>() };
        return _libraries.emplace(libName, SwfFile::read({ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() })).first->second;
    }
}
