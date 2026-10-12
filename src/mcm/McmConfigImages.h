#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>

#include "SwfImages.h"
#include "model/Definition.h"

namespace mods_config::mcm
{
    /**
     * The images of an MCM config. An image row names a mod and a class: the image is that class of the
     * `lib.swf` in the mod's folder of MCM's config folder.
     *
     *     McmConfigImages images(configFolder);
     *     const auto logo = images.load("SS2", "Logo");
     *
     * A file is read once, for all the images of it, so one of these is kept while a config is loaded.
     */
    class McmConfigImages
    {
    public:
        explicit McmConfigImages(std::filesystem::path configFolder);

        model::Image load(const std::string& libName, const std::string& className);

    private:
        const std::expected<SwfFile, std::string>& libraryOf(const std::string& libName);

        // MCM's config folder, which has a folder for each mod
        std::filesystem::path _configFolder;

        // the image file of each mod that was asked for, or why it is not read
        std::map<std::string, std::expected<SwfFile, std::string>> _libraries;
    };
}
