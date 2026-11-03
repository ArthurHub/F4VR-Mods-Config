#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

// A choice of the files of a folder: the files a folder has, the options they are, and how a value is matched to one
// of them. A config spec's value is a path in the form of its default, and an MCM `dropdownFiles`'s a name or a place.

namespace mods_config::model
{
    /**
     * The folders a config spec's files and its choices of files are read from, by their roots: the game's Data folder
     * and `Documents\My Games\Fallout4VR`. With none, as in a test of a config spec alone, none is read.
     */
    struct FileFolders
    {
        std::filesystem::path data;
        std::filesystem::path myGames;
    };

    /**
     * A file's name as a choice of files shows it: its name, and its extension, which is muted after it. A mask that
     * names one extension, as `*.dds`, has it hidden, as every file has it.
     */
    struct ShownFileName
    {
        std::string_view name;
        std::string_view extension;
    };

    std::vector<std::string> listFiles(const std::filesystem::path& folder, std::string_view mask);
    std::filesystem::path folderOf(const FolderOptions& folder, const FileFolders& folders);
    std::string folderText(const FolderOptions& folder);
    std::string fileNameOf(std::string_view value);
    ShownFileName shownFileName(std::string_view name, std::string_view mask);
    bool isInFolder(std::string_view value, const FolderOptions& folder);
    std::vector<Option> fileOptions(const Setting& setting, std::span<const std::string> files);
    std::optional<std::size_t> findFileOption(const std::vector<Option>& options, const FolderOptions& folder, std::string_view value);
    std::string noFilesReason(const FolderOptions& folder);
}
