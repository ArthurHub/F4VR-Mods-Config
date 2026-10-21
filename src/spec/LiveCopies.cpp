#include "LiveCopies.h"

#include <chrono>
#include <format>
#include <fstream>
#include <sstream>

namespace
{
    // a copy that was written this recently is not read yet, as an editor can still be writing it
    constexpr auto QUIET_TIME = std::chrono::milliseconds(200);

    /**
     * A folder's name as a path: the model has it in UTF-8.
     */
    std::filesystem::path folderPath(const std::string_view folder)
    {
        return { std::u8string(folder.begin(), folder.end()) };
    }

    /**
     * The text of a file, and none for one that cannot be read.
     */
    std::string readText(const std::filesystem::path& file)
    {
        std::ostringstream text;
        text << std::ifstream(file, std::ios::binary).rdbuf();
        return text.str();
    }
}

namespace mods_config::spec
{
    LiveCopies::LiveCopies(std::filesystem::path installedFolder, std::filesystem::path copiesFolder)
        : _installedFolder(std::move(installedFolder)),
          _copiesFolder(std::move(copiesFolder))
    {}

    /**
     * The live copy of a config spec's `spec.json`, whether or not there is one.
     */
    std::filesystem::path LiveCopies::copyOf(const std::string_view folder) const
    {
        return _copiesFolder / folderPath(folder) / "spec.json";
    }

    /**
     * A config spec's installed `spec.json`.
     */
    std::filesystem::path LiveCopies::installedOf(const std::string_view folder) const
    {
        return _installedFolder / folderPath(folder) / "spec.json";
    }

    bool LiveCopies::isOn(const std::string_view folder) const
    {
        return _on.contains(folder);
    }

    /**
     * Reads a config spec from its live copy from now on, and copies its installed `spec.json` there first where
     * there is no copy: a copy that is there has its author's edits, and is kept. The error says why there is no
     * copy, and the config spec is not read from it then. A copy is read as it is written next.
     */
    std::expected<void, std::string> LiveCopies::turnOn(const std::string_view folder)
    {
        const auto copy = copyOf(folder);
        std::error_code error;
        if (!std::filesystem::exists(copy, error)) {
            std::filesystem::create_directories(copy.parent_path(), error);
            if (!error) {
                std::filesystem::copy_file(installedOf(folder), copy, error);
            }
            if (error) {
                return std::unexpected(std::format("its spec.json cannot be copied: {}", error.message()));
            }
        }
        _on.emplace(folder, std::filesystem::file_time_type::min());
        return {};
    }

    /**
     * Reads a config spec from its installed `spec.json` again. Its copy is kept.
     */
    void LiveCopies::turnOff(const std::string_view folder)
    {
        if (const auto on = _on.find(folder); on != _on.end()) {
            _on.erase(on);
        }
    }

    /**
     * Copies a config spec's installed `spec.json` over its live copy, which loses the copy's edits. The error says
     * why it is not copied, and the copy is as it was then.
     */
    std::expected<void, std::string> LiveCopies::overwrite(const std::string_view folder)
    {
        const auto copy = copyOf(folder);
        std::error_code error;
        std::filesystem::create_directories(copy.parent_path(), error);
        if (!error) {
            std::filesystem::copy_file(installedOf(folder), copy, std::filesystem::copy_options::overwrite_existing, error);
        }
        if (error) {
            return std::unexpected(std::format("its spec.json cannot be copied: {}", error.message()));
        }
        return {};
    }

    /**
     * Whether a config spec's installed `spec.json` was written after its live copy, with another text: the copy
     * does not have what changed in it. A copy has the time of the file it is copied from, so a copy the author did
     * not edit since is not. One that is not there is not either.
     */
    bool LiveCopies::isStale(const std::string_view folder) const
    {
        std::error_code error;
        const auto installed = std::filesystem::last_write_time(installedOf(folder), error);
        const auto copied = error ? installed : std::filesystem::last_write_time(copyOf(folder), error);
        return !error && installed > copied && readText(installedOf(folder)) != readText(copyOf(folder));
    }

    /**
     * Keeps the time a config spec's copy was written as it is read now, so it is not read again until it is written.
     */
    void LiveCopies::markRead(const std::string_view folder)
    {
        if (const auto on = _on.find(folder); on != _on.end()) {
            std::error_code error;
            on->second = std::filesystem::last_write_time(copyOf(folder), error);
        }
    }

    /**
     * The folders of the config specs whose copies were written since they were read, and not in the last moment, as
     * an editor can still be writing one. A copy that is gone is not.
     */
    std::vector<std::string> LiveCopies::changed() const
    {
        std::vector<std::string> changed;
        const auto now = std::filesystem::file_time_type::clock::now();
        for (const auto& [folder, read] : _on) {
            std::error_code error;
            const auto written = std::filesystem::last_write_time(copyOf(folder), error);
            if (!error && written != read && now - written >= QUIET_TIME) {
                changed.push_back(folder);
            }
        }
        return changed;
    }
}
