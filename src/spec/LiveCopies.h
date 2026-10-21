#pragma once

#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mods_config::spec
{
    /**
     * The live copies of config specs' `spec.json`, for their authors: a copy of each in a folder of its own, which
     * the menu reads in place of the installed file while the config spec's switch is on, and reads again each time
     * it is saved. A config spec is named by its folder in `Data\ModsConfigVR\Config`, which its copy's folder has
     * too. The switches are kept for this run of the game, and a copy is kept on the disk.
     */
    class LiveCopies
    {
    public:
        LiveCopies(std::filesystem::path installedFolder, std::filesystem::path copiesFolder);

        std::filesystem::path copyOf(std::string_view folder) const;
        bool isOn(std::string_view folder) const;
        std::expected<void, std::string> turnOn(std::string_view folder);
        void turnOff(std::string_view folder);
        std::expected<void, std::string> overwrite(std::string_view folder);
        bool isStale(std::string_view folder) const;
        void markRead(std::string_view folder);
        std::vector<std::string> changed() const;

    private:
        std::filesystem::path installedOf(std::string_view folder) const;

        // the config specs' folders, `Data\ModsConfigVR\Config`, and the folder of their copies
        std::filesystem::path _installedFolder;
        std::filesystem::path _copiesFolder;

        // the config specs that are read from their copies, by their folders, with the time their copy was written
        // as it was read last
        std::map<std::string, std::filesystem::file_time_type, std::less<>> _on;
    };
}
