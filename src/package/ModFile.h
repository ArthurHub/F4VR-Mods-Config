#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "model/Definition.h"

namespace mods_config::package
{
    /**
     * A file of a package's mod, a TOML or an INI file, which the package store reads its values from and
     * writes them into. A value is named by its path: the tables it is in and its key, or its section and its
     * key. A write changes the values alone, and keeps the rest of the file as it is.
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
        };

        virtual ~ModFile() = default;

        /**
         * Reads the file's values. A file that is not there, cannot be read or is not written right has none,
         * and the reason is returned.
         */
        virtual std::expected<void, std::string> read() = 0;

        /**
         * Whether the file has a value at a path, of any type, as it was last read or written.
         */
        virtual bool contains(std::span<const std::string> path) const = 0;

        /**
         * The value at a path in a type, as the file was last read or written, or the reason it is not one.
         */
        virtual std::expected<model::Value, std::string> value(std::span<const std::string> path, model::StoredType type) const = 0;

        /**
         * Writes values into the file, which is read again first, in one write, or not at all, with the reason.
         */
        virtual std::expected<void, std::string> write(std::span<const Change> changes) = 0;

        /**
         * A path as a reason names it.
         */
        virtual std::string pathName(std::span<const std::string> path) const = 0;
    };
}
