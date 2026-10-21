#pragma once

#include <expected>
#include <span>
#include <string>

#include <toml++/toml.hpp>

#include "ModFile.h"

namespace mods_config::spec
{
    /**
     * A TOML file of a config spec's mod. A value is read by its path, in the type of its setting, and new values are
     * written into the file's text where the old ones are, with the framework's `common::TomlEditor`: its comments,
     * its blank lines, the order of its keys and how its other values are written stay as they are.
     */
    class TomlFile : public ModFile
    {
    public:
        using ModFile::ModFile;

        bool contains(std::span<const std::string> path) const override;
        std::expected<model::Value, std::string> value(std::span<const std::string> path, model::StoredType type) const override;
        std::string pathName(std::span<const std::string> path) const override;

    protected:
        std::expected<void, std::string> load(const std::string& text) override;
        std::expected<std::string, std::string> edit(const std::string& text, std::span<const Change> changes) const override;

    private:
        toml::node_view<const toml::node> find(std::span<const std::string> path) const;

        // the values of the file as it was last read or written. Empty before a read, and after one that failed
        toml::table _table;
    };
}
