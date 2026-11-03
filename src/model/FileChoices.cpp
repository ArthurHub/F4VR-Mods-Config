#include "FileChoices.h"

#include <algorithm>
#include <format>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    // what a path of a choice of files starts with that is from the game's Data folder, as a texture's path can
    constexpr std::string_view DATA_PREFIX = "data\\";

    /**
     * Where a value's folder ends: the place of its last separator, `\` or `/`, and npos for a bare name.
     */
    std::size_t lastSeparator(const std::string_view value)
    {
        return value.find_last_of("\\/");
    }

    /**
     * A folder as a choice of files compares one with another: in lower case, as Windows matches a name, with `\`
     * between its folders and none at its end. A folder of the Data folder can start with `data\`, which is dropped.
     */
    std::string comparableFolder(const std::string_view folder, const model::FileRoot root)
    {
        auto text = common::str_tolower(std::string(folder));
        std::ranges::replace(text, '/', '\\');
        while (text.ends_with('\\')) {
            text.pop_back();
        }
        if (root == model::FileRoot::Data && text.starts_with(DATA_PREFIX)) {
            text.erase(0, DATA_PREFIX.size());
        }
        return text;
    }

    /**
     * The extension a mask names for every file it lists, as `.dds` of `*.dds` and of `activation-icon-*.dds`: empty
     * for a mask that lists files of more than one, as `*` and `*.d?s`.
     */
    std::string_view maskExtension(const std::string_view mask)
    {
        const auto dot = mask.rfind('.');
        if (dot == std::string_view::npos) {
            return {};
        }
        const auto extension = mask.substr(dot);
        return extension.find_first_of("*?") == std::string_view::npos ? extension : std::string_view();
    }
}

namespace mods_config::model
{
    /**
     * The names of the files of a folder that match a mask, alphabetical with no regard to case. The mask is the
     * system's, `*` and `?`, and an empty one lists every file. The folder alone is listed, and a folder in it is no
     * file. None for a folder that is not there.
     */
    std::vector<std::string> listFiles(const std::filesystem::path& folder, const std::string_view mask)
    {
        std::vector<std::string> files;
        WIN32_FIND_DATAW found{};
        const std::string wildcard(mask.empty() ? std::string_view("*") : mask);
        const auto pattern = folder / std::filesystem::path(std::u8string(wildcard.begin(), wildcard.end()));
        const auto search = FindFirstFileW(pattern.c_str(), &found);
        if (search == INVALID_HANDLE_VALUE) {
            return files;
        }
        do {
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                const auto name = std::filesystem::path(found.cFileName).u8string();
                files.emplace_back(name.begin(), name.end());
            }
        } while (FindNextFileW(search, &found));
        FindClose(search);
        std::ranges::sort(files, {}, common::str_tolower);
        return files;
    }

    /**
     * Where the folder of a choice of files is, from the folder of its root. Empty while that is not known, as in a
     * test of a config spec alone.
     */
    std::filesystem::path folderOf(const FolderOptions& folder, const FileFolders& folders)
    {
        const auto& root = folder.root == FileRoot::Data ? folders.data : folders.myGames;
        return root.empty() ? std::filesystem::path() : root / std::filesystem::path(std::u8string(folder.path.begin(), folder.path.end()));
    }

    /**
     * The folder of a choice of files as the player reads it: its path, with `\` between its folders.
     */
    std::string folderText(const FolderOptions& folder)
    {
        auto text = folder.path;
        std::ranges::replace(text, '/', '\\');
        while (text.ends_with('\\')) {
            text.pop_back();
        }
        return text;
    }

    /**
     * The name of the file a value of a choice of files names: what is after its last separator.
     */
    std::string fileNameOf(const std::string_view value)
    {
        const auto separator = lastSeparator(value);
        return std::string(separator == std::string_view::npos ? value : value.substr(separator + 1));
    }

    /**
     * A file's name as a choice of files shows it: with no extension where its mask names one, which every file has,
     * and else with the extension after its last dot, which is muted.
     */
    ShownFileName shownFileName(const std::string_view name, const std::string_view mask)
    {
        const auto fixed = maskExtension(mask);
        if (!fixed.empty()) {
            const bool ends = name.size() > fixed.size() && common::str_tolower(std::string(name.substr(name.size() - fixed.size()))) == common::str_tolower(std::string(fixed));
            return { .name = ends ? name.substr(0, name.size() - fixed.size()) : name, .extension = {} };
        }
        const auto dot = name.rfind('.');
        if (dot == std::string_view::npos || dot == 0) {
            return { .name = name, .extension = {} };
        }
        return { .name = name.substr(0, dot), .extension = name.substr(dot) };
    }

    /**
     * Whether a value of a choice of files is in its folder: a bare name is, and so is a path whose folder is the
     * folder, with no regard to case or to which separator it has, and from the Data folder with or without `data\`.
     */
    bool isInFolder(const std::string_view value, const FolderOptions& folder)
    {
        const auto separator = lastSeparator(value);
        return separator == std::string_view::npos || comparableFolder(value.substr(0, separator), folder.root) == comparableFolder(folder.path, folder.root);
    }

    /**
     * The options of a config spec's choice of files, of the files its folder has: the one that is no file first, for
     * a choice that has it, and a file each, its name as its text, and as its value the path of its default with the
     * file's name in place of the default's. A setting with no default, or whose default is the empty text of no file,
     * has its folder's path before the name.
     */
    std::vector<Option> fileOptions(const Setting& setting, const std::span<const std::string> files)
    {
        const auto& folder = *setting.valueOptions.folderOptions;
        std::vector<Option> options;
        if (!folder.noneText.empty()) {
            options.push_back({ .value = std::string(), .text = folder.noneText });
        }
        const auto* defaultText = setting.defaultValue ? std::get_if<std::string>(&*setting.defaultValue) : nullptr;
        std::string prefix;
        if (defaultText && !defaultText->empty()) {
            const auto separator = lastSeparator(*defaultText);
            prefix = separator == std::string_view::npos ? std::string() : defaultText->substr(0, separator + 1);
        } else if (!folder.path.empty()) {
            // with the separator its path has
            const char separator = folder.path.find('\\') == std::string::npos && folder.path.find('/') != std::string::npos ? '/' : '\\';
            prefix = folder.path.ends_with('\\') || folder.path.ends_with('/') ? folder.path : folder.path + separator;
        }
        for (const auto& file : files) {
            options.push_back({ .value = prefix + file, .text = file });
        }
        return options;
    }

    /**
     * The option of a choice of files a value names, by the place of the file's name in its folder, with no regard to
     * case, where the value is in the folder (`isInFolder`). An empty value is the option that is no file. None for a
     * value of no file of the folder.
     */
    std::optional<std::size_t> findFileOption(const std::vector<Option>& options, const FolderOptions& folder, const std::string_view value)
    {
        if (!value.empty() && !isInFolder(value, folder)) {
            return std::nullopt;
        }
        const auto name = common::str_tolower(fileNameOf(value));
        for (std::size_t place = 0; place < options.size(); ++place) {
            const auto* text = std::get_if<std::string>(&options[place].value);
            if (text && common::str_tolower(fileNameOf(*text)) == name) {
                return place;
            }
        }
        return std::nullopt;
    }

    /**
     * Why a choice of files whose folder has none of them has no options: where it looked, as `Not available: no *.dds
     * files in Textures\MyMod\Gobos.`
     */
    std::string noFilesReason(const FolderOptions& folder)
    {
        return folder.mask.empty() ? std::format("Not available: no files in {}.", folderText(folder))
                                   : std::format("Not available: no {} files in {}.", folder.mask, folderText(folder));
    }
}
