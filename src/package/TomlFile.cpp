#include "TomlFile.h"

#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <string_view>
#include <variant>

#include "common/TomlEditor.h"
#include "config/ConfigFile.h"

using namespace mods_config;

namespace
{
    /**
     * A path as TOML writes it as a dotted key, `Panel.fScale`, for a reason.
     */
    std::string pathText(const std::span<const std::string> path)
    {
        return path | std::views::join_with('.') | std::ranges::to<std::string>();
    }

    /**
     * What a value of a type is, for a reason.
     */
    std::string_view typeName(const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            return "true or false";
        case model::StoredType::Int:
            return "a whole number";
        case model::StoredType::Float:
            return "a number";
        case model::StoredType::String:
            return "a text";
        }
        return {};
    }

    /**
     * A node's value in a type, or no value when it is not of it. A whole number is a number too, and a whole
     * number has to fit in an int.
     */
    std::optional<model::Value> valueOf(const toml::node& node, const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            if (const auto* value = node.as_boolean()) {
                return model::Value(value->get());
            }
            break;
        case model::StoredType::Int:
            if (const auto* value = node.as_integer(); value && value->get() >= std::numeric_limits<int>::min() && value->get() <= std::numeric_limits<int>::max()) {
                return model::Value(static_cast<int>(value->get()));
            }
            break;
        case model::StoredType::Float:
            if (const auto* value = node.as_floating_point()) {
                return model::Value(static_cast<float>(value->get()));
            }
            if (const auto* value = node.as_integer()) {
                return model::Value(static_cast<float>(value->get()));
            }
            break;
        case model::StoredType::String:
            if (const auto* value = node.as_string()) {
                return model::Value(value->get());
            }
            break;
        }
        return std::nullopt;
    }

    /**
     * A node as a reason shows it: a text in quotes, with no escapes, and any other value as TOML writes it.
     */
    std::string shownText(const toml::node& node)
    {
        if (const auto* text = node.as_string()) {
            return std::format("\"{}\"", text->get());
        }
        return common::TomlEditor::Value(node).text();
    }

    /**
     * The whole text of a file, or the reason it is not there or cannot be read.
     */
    std::expected<std::string, std::string> readText(const std::filesystem::path& path)
    {
        auto text = f4cf::config::ConfigFile::readFile(path.string());
        if (!text) {
            const auto name = path.filename().string();
            return std::unexpected(std::filesystem::exists(path) ? std::format("{} cannot be read", name) : std::format("there is no file {}", name));
        }
        return std::move(*text);
    }
}

namespace mods_config::package
{
    /**
     * Reads the file's values. A file that is not there, cannot be read or is no TOML has none, and the reason
     * is returned: a parse error says what is wrong, at which line and column.
     */
    std::expected<void, std::string> TomlFile::read()
    {
        _table = {};
        const auto text = readText(_path);
        if (!text) {
            return std::unexpected(text.error());
        }
        auto table = common::parseToml(*text, name());
        if (!table) {
            return std::unexpected(std::move(table.error()));
        }
        _table = std::move(*table);
        return {};
    }

    /**
     * Whether the file has a value at a path of tables and a key, of any type, as it was last read or written.
     */
    bool TomlFile::contains(const std::span<const std::string> path) const
    {
        return static_cast<bool>(find(path));
    }

    /**
     * The value at a path of tables and a key, `["Panel", "fScale"]`, in a type, as the file was last read or
     * written. A value the file does not have, or that is not of the type, is not one, and the reason is
     * returned. A whole number is read as a number, and a number with a fraction is not a whole number.
     */
    std::expected<model::Value, std::string> TomlFile::value(const std::span<const std::string> path, const model::StoredType type) const
    {
        const auto node = find(path);
        if (!node) {
            return std::unexpected(std::format("{} is not in {}", pathText(path), name()));
        }
        auto value = valueOf(*node.node(), type);
        if (!value) {
            const auto what = type == model::StoredType::Int && node.is_integer() ? "which is too large"s : std::format("which is not {}", typeName(type));
            return std::unexpected(std::format("{} is {} in {}, {}", pathText(path), shownText(*node.node()), name(), what));
        }
        return std::move(*value);
    }

    /**
     * Writes values into the file. It is read again first, so a value someone else changed since it was read
     * stays, but for one that is written here. Each value replaces the text of the one the file has, and one it
     * does not have is added. A file that has every value already is not written. When one of them cannot be
     * written, or the file cannot be read or is no TOML, it is not written at all, and the reason is returned.
     */
    std::expected<void, std::string> TomlFile::write(const std::span<const Change> changes)
    {
        const auto text = readText(_path);
        if (!text) {
            return std::unexpected(text.error());
        }
        common::TomlEditor editor(*text, name());
        if (!editor.ok()) {
            return std::unexpected(editor.error());
        }

        std::vector<common::TomlEditor::Edit> edits;
        edits.reserve(changes.size());
        for (const auto& [path, value] : changes) {
            edits.push_back({ { path.begin(), path.end() },
                std::visit(
                    [](const auto& typed) {
                        return common::TomlEditor::Value(typed);
                    },
                    value) });
        }
        if (!editor.set(edits)) {
            return std::unexpected(editor.error());
        }
        if (editor.text() != *text && !f4cf::config::ConfigFile::writeFile(_path.string(), editor.text())) {
            return std::unexpected(std::format("{} cannot be written", name()));
        }
        _table = editor.table();
        return {};
    }

    /**
     * A path as a reason names it, as TOML writes it as a dotted key: `Panel.fScale`.
     */
    std::string TomlFile::pathName(const std::span<const std::string> path) const
    {
        return pathText(path);
    }

    /**
     * The node at a path of tables and a key, which is empty where the file has none.
     */
    toml::node_view<const toml::node> TomlFile::find(const std::span<const std::string> path) const
    {
        toml::node_view<const toml::node> node(_table);
        for (const auto& key : path) {
            node = node[key];
        }
        return node;
    }

    /**
     * The file's name, which names it in a reason.
     */
    std::string TomlFile::name() const
    {
        return _path.filename().string();
    }
}
