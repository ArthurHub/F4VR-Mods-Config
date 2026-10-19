#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>

#include <toml++/toml.hpp>

#include "ModFile.h"

namespace mods_config::package
{
    /**
     * A TOML file of a package's mod. A value is read by its path, in the type of its setting, and new values are
     * written into the file's text where the old ones are, with the framework's `common::TomlEditor`: its comments,
     * its blank lines, the order of its keys and how its other values are written stay as they are.
     */
    class TomlFile : public ModFile
    {
    public:
        explicit TomlFile(std::filesystem::path path)
            : _path(std::move(path))
        {}

        std::expected<void, std::string> read() override;
        bool contains(std::span<const std::string> path) const override;
        std::expected<model::Value, std::string> value(std::span<const std::string> path, model::StoredType type) const override;
        std::expected<void, std::string> write(std::span<const Change> changes) override;
        std::string pathName(std::span<const std::string> path) const override;

    private:
        toml::node_view<const toml::node> find(std::span<const std::string> path) const;
        std::string name() const;

        // the file, absolute
        std::filesystem::path _path;

        // the values of the file as it was last read or written. Empty before a read, and after one that failed
        toml::table _table;
    };
}
