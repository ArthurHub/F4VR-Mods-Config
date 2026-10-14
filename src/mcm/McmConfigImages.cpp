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

    // how large an M8r intro shows the logo it loads: its `EmbeddedImage` scales its bitmap by 1280 / 1920
    constexpr float INTRO_LOGO_SCALE = 1280.0f / 1920.0f;
}

namespace mods_config::mcm
{
    McmConfigImages::McmConfigImages(std::filesystem::path configFolder)
        : _configFolder(std::move(configFolder))
    {}

    /**
     * The image of a row, a class of the mod's `lib.swf`, at the size MCM's menu shows it.
     * An image that cannot be read has no pixels, with the reason in the log.
     */
    model::Image McmConfigImages::load(const std::string& libName, const std::string& className)
    {
        const auto& library = fileOf(libName, IMAGE_LIBRARY_FILE);
        return fitted(library ? library->image(className) : std::unexpected(library.error()), className, libName);
    }

    /**
     * The logo an M8r intro loads: the image of the class of a file of the mod's config folder itself, at
     * the size MCM's menu shows it. The class is M8r's `EmbeddedImage`, which shows its bitmap at 2/3 of
     * its size in each intro of the MCM mods.
     * An image that cannot be read has no pixels, with the reason in the log.
     */
    model::Image McmConfigImages::loadMain(const std::string& libName, const std::string& fileName)
    {
        const auto& file = fileOf(libName, fileName);
        if (!file) {
            return fitted(std::unexpected(file.error()), fileName, libName);
        }
        const auto mainClass = file->mainClass();
        auto image = mainClass.empty() ? std::expected<model::Image, std::string>(std::unexpected("The file has no class of its own"s)) : file->image(mainClass);
        if (image) {
            image->width *= INTRO_LOGO_SCALE;
            image->height *= INTRO_LOGO_SCALE;
        }
        return fitted(image, fileName, libName);
    }

    /**
     * An image as large as it is, and smaller where that is more than MCM's menu has room for. One that is not
     * read has no pixels, and the log has why, with the name of what it is.
     */
    model::Image McmConfigImages::fitted(const std::expected<model::Image, std::string>& image, const std::string& name, const std::string& libName) const
    {
        if (!image) {
            logger::warn("MCM image '{}' of '{}' is not shown: {}", name, libName, image.error());
            return {};
        }
        auto shown = *image;
        const float fit = (std::min)({ 1.0f, MAX_IMAGE_WIDTH / shown.width, MAX_IMAGE_HEIGHT / shown.height });
        shown.width *= fit;
        shown.height *= fit;
        return shown;
    }

    /**
     * A file of a mod's config folder, by the name of the folder. It is read when it is first asked for, and
     * a file that is not there or is not read is asked for once too.
     */
    const std::expected<SwfFile, std::string>& McmConfigImages::fileOf(const std::string& libName, const std::string& fileName)
    {
        const auto key = std::pair(libName, fileName);
        if (const auto found = _files.find(key); found != _files.end()) {
            return found->second;
        }
        // a name of a config is UTF-8, which a path takes as such only from a u8string
        const auto toPath = [](const std::string& name) {
            return std::filesystem::path(std::u8string(name.begin(), name.end()));
        };
        if (fileName.empty()) {
            return _files.emplace(key, std::unexpected("The row names no file"s)).first->second;
        }
        const auto file = _configFolder / toPath(libName) / toPath(fileName);
        std::error_code noFile;
        if (libName.empty() || _configFolder.empty() || !std::filesystem::is_regular_file(file, noFile)) {
            return _files.emplace(key, std::unexpected(std::format("The mod has no {}", fileName))).first->second;
        }
        std::ifstream stream(file, std::ios::binary);
        const std::vector<char> bytes{ std::istreambuf_iterator(stream), std::istreambuf_iterator<char>() };
        return _files.emplace(key, SwfFile::read({ reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size() })).first->second;
    }
}
