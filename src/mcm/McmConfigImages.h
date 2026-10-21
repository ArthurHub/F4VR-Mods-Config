#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <utility>

#include "SwfImages.h"
#include "model/ConfigDefinition.h"

namespace mods_config::mcm
{
    /**
     * The images of an MCM config. An image row names a mod and a class: the image is that class of the
     * `lib.swf` in the mod's folder of MCM's config folder. The code of some mods loads its picture from
     * another file of that folder: the image is then the class of that file itself.
     *
     *     McmConfigImages images(configFolder);
     *     const auto logo = images.load("SS2", "Logo");
     *     const auto intro = images.loadMain("GlobalStashBySKK", "logo.swf");
     *
     * A file is read once, for all the images of it, so one of these is kept while a config is loaded.
     */
    class McmConfigImages
    {
    public:
        explicit McmConfigImages(std::filesystem::path configFolder);

        model::Image load(const std::string& libName, const std::string& className);
        model::Image loadMain(const std::string& libName, const std::string& fileName);

    private:
        model::Image fitted(const std::expected<model::Image, std::string>& image, const std::string& name, const std::string& libName) const;
        const std::expected<SwfFile, std::string>& fileOf(const std::string& libName, const std::string& fileName);

        // MCM's config folder, which has a folder for each mod
        std::filesystem::path _configFolder;

        // each file of a mod that was asked for, by the mod and the file's name, or why it is not read
        std::map<std::pair<std::string, std::string>, std::expected<SwfFile, std::string>> _files;
    };
}
