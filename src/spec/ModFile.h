#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "model/ConfigDefinition.h"

namespace mods_config::spec
{
    /**
     * A file of a config spec's mod, a TOML or an INI file, which the config spec store reads its values from and
     * writes them into. A value is named by its path: the tables it is in and its key, or its section and its
     * key. A write changes the values alone, and keeps the rest of the file as it is.
     * A Save writes its files in three steps, so one that fails leaves every file as it was: `prepare` makes
     * the text of each with nothing written, `commit` writes it, and `restore` puts back one that was written.
     */
    class ModFile
    {
    public:
        /**
         * A value to write, at its path.
         */
        struct Change
        {
            std::vector<std::string> path;
            model::Value value;

            // the value is the TOML text of a table, as a binding's, which is written as the table and not as a text
            bool table = false;

            // the key is removed from the file, with its value, and one the file does not have stays so: the value is
            // not written. Only a TOML file removes a key
            bool remove = false;
        };

        explicit ModFile(std::filesystem::path path)
            : _path(std::move(path))
        {}

        virtual ~ModFile() = default;

        std::expected<void, std::string> read();
        std::expected<void, std::string> prepare(std::span<const Change> changes);
        std::expected<void, std::string> commit();
        std::expected<void, std::string> restore();

        /**
         * Whether the file has a value at a path, of any type, as it was last read or written.
         */
        virtual bool contains(std::span<const std::string> path) const = 0;

        /**
         * The value at a path in a type, as the file was last read or written, or the reason it is not one.
         */
        virtual std::expected<model::Value, std::string> value(std::span<const std::string> path, model::StoredType type) const = 0;

        /**
         * The value at a path as the TOML text it is written as, whatever its type, as a binding's table, or the
         * reason it is none. Only a TOML file has one.
         */
        virtual std::expected<std::string, std::string> tableText(std::span<const std::string> path) const
        {
            return std::unexpected(std::format("{} is not in a TOML file", pathName(path)));
        }

        /**
         * A path as a reason names it.
         */
        virtual std::string pathName(std::span<const std::string> path) const = 0;

    protected:
        /**
         * Takes the values of a text as the file's, or has none and returns the reason, for a text that is not
         * written right. An empty text has none.
         */
        virtual std::expected<void, std::string> load(const std::string& text) = 0;

        /**
         * A text with the values set in it, checked to read back as them, or the reason they cannot be set. The
         * rest of the text stays as it is.
         */
        virtual std::expected<std::string, std::string> edit(const std::string& text, std::span<const Change> changes) const = 0;

        std::string name() const;

    private:
        // the file, absolute
        std::filesystem::path _path;

        // of the Save that is under way: the text `prepare` read, none for a file that is not there, and the
        // text it made
        std::optional<std::string> _before;
        std::string _after;

        // whether `commit` wrote the file, so `restore` puts it back
        bool _written = false;
    };
}
