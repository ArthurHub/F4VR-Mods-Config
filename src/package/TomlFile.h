#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "model/Definition.h"

namespace mods_config::package
{
    /**
     * A TOML file of a package's mod. A value is read by its path, in the type of its setting, and new values are
     * written into the file's text where the old ones are, with the framework's `common::TomlEditor`: its comments,
     * its blank lines, the order of its keys and how its other values are written stay as they are.
     */
    class TomlFile
    {
    public:
        /**
         * A value to write, at the path of its tables and its key.
         */
        struct Change
        {
            std::vector<std::string> path;
            model::Value value;
        };

        explicit TomlFile(std::filesystem::path path)
            : _path(std::move(path))
        {}

        std::expected<void, std::string> read();
        bool contains(std::span<const std::string> path) const;
        std::expected<model::Value, std::string> value(std::span<const std::string> path, model::StoredType type) const;
        std::expected<void, std::string> write(std::span<const Change> changes);

    private:
        toml::node_view<const toml::node> find(std::span<const std::string> path) const;
        std::string name() const;

        // the file, absolute
        std::filesystem::path _path;

        // the values of the file as it was last read or written. Empty before a read, and after one that failed
        toml::table _table;
    };
}
