#include "ModFile.h"

#include <format>
#include <tuple>

#include "config/ConfigFile.h"

namespace mods_config::spec
{
    /**
     * Reads the file's values. A file that is not there, cannot be read or is not written right has none, and
     * the reason is returned.
     */
    std::expected<void, std::string> ModFile::read()
    {
        auto text = f4cf::config::ConfigFile::readFile(_path.string());
        if (!text) {
            std::ignore = load({});
            return std::unexpected(std::filesystem::exists(_path) ? std::format("{} cannot be read", name()) : std::format("there is no file {}", name()));
        }
        return load(*text);
    }

    /**
     * The first steps of a Save: reads the file again, so a value someone else changed since it was read stays
     * but for one that is written here, sets the values in its text and checks that they read back. A file
     * that is not there is an empty one. Nothing is written, and the file's values stay as they are.
     * Returns the reason for a file that cannot be read, is not written right or cannot have a value.
     */
    std::expected<void, std::string> ModFile::prepare(const std::span<const Change> changes)
    {
        _before.reset();
        _after.clear();
        _written = false;
        if (std::filesystem::exists(_path)) {
            _before = f4cf::config::ConfigFile::readFile(_path.string());
            if (!_before) {
                return std::unexpected(std::format("{} cannot be read", name()));
            }
        }
        auto edited = edit(_before.value_or(std::string()), changes);
        if (!edited) {
            return std::unexpected(std::move(edited.error()));
        }
        _after = std::move(*edited);
        return {};
    }

    /**
     * Writes the text `prepare` made over the file in one write, and creates a file that is not there, with the
     * folders on the way to it. A file that has the text already is not written. The file's values are then
     * the ones written. Returns the reason for a file that cannot be written.
     */
    std::expected<void, std::string> ModFile::commit()
    {
        if (_before == _after) {
            return load(_after);
        }
        std::error_code error;
        std::filesystem::create_directories(_path.parent_path(), error);
        if (!f4cf::config::ConfigFile::writeFile(_path.string(), _after)) {
            return std::unexpected(std::format("{} cannot be written", name()));
        }
        _written = true;
        return load(_after);
    }

    /**
     * Puts back the file that `commit` wrote, for a Save that fails after it: the text `prepare` read, or no
     * file for one that was not there. The folders the write created stay. The file's values are then the
     * ones put back. Returns the reason for a file that cannot be put back.
     */
    std::expected<void, std::string> ModFile::restore()
    {
        if (!_written) {
            return {};
        }
        _written = false;
        if (!_before) {
            std::error_code error;
            std::filesystem::remove(_path, error);
            std::ignore = load({});
            if (error) {
                return std::unexpected(std::format("{} cannot be removed: {}", name(), error.message()));
            }
            return {};
        }
        if (!f4cf::config::ConfigFile::writeFile(_path.string(), *_before)) {
            return std::unexpected(std::format("{} cannot be written", name()));
        }
        return load(*_before);
    }

    /**
     * The file's name, which names it in a reason.
     */
    std::string ModFile::name() const
    {
        return _path.filename().string();
    }
}
