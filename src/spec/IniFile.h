#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ModFile.h"

namespace mods_config::spec
{
    /**
     * An INI file of a config spec's mod, read as CommonFramework mods and SimpleIni read one. A value is named by
     * its section and its key, or by its key alone before the first section, with no regard to case, and is the
     * text after its `=`, with no spaces around it. A line that starts with `;` or `#` is a comment, and so is a
     * `;` or `#` after a value with a space before it, as `8.5  # target`, which SimpleIni reads as part of the
     * value. A key the file has more than once has the value of its last, and a write sets each of them. A write
     * changes the text of the values alone: the comments, trailing ones too, the blank lines, the order of the
     * keys and every other line stay as they are.
     */
    class IniFile : public ModFile
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
        /**
         * A line of the file's text.
         */
        struct Line
        {
            enum class Kind : std::uint8_t
            {
                // a comment, a blank line, or any other line that is no section and no value
                Other,
                Section,
                Value,
            };

            Kind kind = Kind::Other;

            // the section the line is in, or the one it starts. Empty before the first section
            std::string section;

            // the key of a value
            std::string key;

            // of a value: its `=` with the spaces around it, as `=` or ` = `
            std::string separator;

            // where the value is in the text, with no spaces around it
            std::size_t valueBegin = 0;
            std::size_t valueEnd = 0;

            // where the next line starts in the text, or its end
            std::size_t next = 0;

            // whether the line ends in a line break, as the file's last line may not
            bool hasBreak = false;
        };

        static std::vector<Line> parse(std::string_view text);
        static const Line* find(const std::vector<Line>& lines, std::span<const std::string> path);
        static void set(std::string& text, std::span<const std::string> path, const model::Value& value);
        std::expected<model::Value, std::string> valueIn(std::string_view text, const std::vector<Line>& lines, std::span<const std::string> path, model::StoredType type) const;

        // the text of the file as it was last read or written, and its lines. Empty before a read, and after
        // one that failed
        std::string _text;
        std::vector<Line> _lines;
    };
}
