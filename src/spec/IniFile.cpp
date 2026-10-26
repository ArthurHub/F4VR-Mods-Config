#include "IniFile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <utility>
#include <variant>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    // the mark a file in UTF-8 can start with, which is no part of its first line
    constexpr std::string_view BOM = "\xEF\xBB\xBF";

    // the spaces around a key, a value and the name of a section
    constexpr std::string_view SPACES = " \t";

    // what a value of a new key is written with: the `=` of a line of another key, or this
    constexpr std::string_view SEPARATOR = "=";

    // the words of true and false an INI file can have, as SimpleIni reads them, with the first as written
    constexpr std::array<std::pair<std::string_view, std::string_view>, 4> BOOL_WORDS{ {
        { "1", "0" },
        { "true", "false" },
        { "yes", "no" },
        { "on", "off" },
    } };

    std::string_view trimmed(const std::string_view text)
    {
        const auto first = text.find_first_not_of(SPACES);
        return first == std::string_view::npos ? std::string_view() : text.substr(first, text.find_last_not_of(SPACES) - first + 1);
    }

    /**
     * Whether two names of an INI file are the same, as SimpleIni and Windows match them: with no regard to case.
     */
    bool sameName(const std::string_view name, const std::string_view other)
    {
        return std::ranges::equal(name, other, [](const char a, const char b) {
            return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
        });
    }

    /**
     * The section and the key a path names: a key alone is before the first section. None for a path of more
     * names, which is no key of an INI file.
     */
    std::optional<std::pair<std::string_view, std::string_view>> keyOf(const std::span<const std::string> path)
    {
        if (path.size() == 1) {
            return std::pair(std::string_view(), std::string_view(path[0]));
        }
        if (path.size() == 2) {
            return std::pair(std::string_view(path[0]), std::string_view(path[1]));
        }
        return std::nullopt;
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
     * The place of a word of true or false among the words, and whether it is true. None for any other text.
     */
    std::optional<std::pair<std::size_t, bool>> boolWord(const std::string_view text)
    {
        for (std::size_t i = 0; i < BOOL_WORDS.size(); ++i) {
            if (sameName(text, BOOL_WORDS[i].first) || sameName(text, BOOL_WORDS[i].second)) {
                return std::pair(i, sameName(text, BOOL_WORDS[i].first));
            }
        }
        return std::nullopt;
    }

    /**
     * Whether a text is a whole number in hex, as `0x15DA`, which SimpleIni reads.
     */
    bool isHex(const std::string_view text)
    {
        return text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X') && text[2] != '-';
    }

    /**
     * The text of a value in a type, or no value when it is not of it. A whole number is in decimals or in hex
     * after `0x`, a whole number is a number too, and it has to fit in an int: `tooLarge` says when it does not.
     */
    std::optional<model::Value> valueOf(const std::string_view text, const model::StoredType type, bool& tooLarge)
    {
        const auto* end = text.data() + text.size();
        switch (type) {
        case model::StoredType::Bool:
            if (const auto word = boolWord(text)) {
                return model::Value(word->second);
            }
            break;
        case model::StoredType::Int: {
            const bool hex = isHex(text);
            int whole = 0;
            const auto [read, error] = std::from_chars(text.data() + (hex ? 2 : 0), end, whole, hex ? 16 : 10);
            tooLarge = error == std::errc::result_out_of_range;
            if (error == std::errc() && read == end) {
                return model::Value(whole);
            }
            break;
        }
        case model::StoredType::Float: {
            float number = 0.0f;
            const auto [read, error] = std::from_chars(text.data(), end, number);
            if (error == std::errc() && read == end && std::isfinite(number)) {
                return model::Value(number);
            }
            break;
        }
        case model::StoredType::String:
            return model::Value(std::string(text));
        }
        return std::nullopt;
    }

    /**
     * The text a value is written with, in place of the old text of its key. A bool is written in the words of
     * the old text and in its case, `true` as `True` or `TRUE`, and as `1` or `0` where the old text is none. A
     * whole number is in hex where the old text is, with as many digits and in its case, unless it is negative.
     * A number is as short as it reads back as the same number, with a decimal point.
     */
    std::string valueText(const model::Value& value, const std::string_view old)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            const auto word = boolWord(old);
            const auto& words = BOOL_WORDS[word ? word->first : 0];
            std::string text(*on ? words.first : words.second);
            const bool upper = std::ranges::any_of(old, [](const char c) {
                return std::isupper(static_cast<unsigned char>(c));
            });
            const bool allUpper = upper && std::ranges::none_of(old, [](const char c) {
                return std::islower(static_cast<unsigned char>(c));
            });
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (allUpper || (upper && i == 0)) {
                    text[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[i])));
                }
            }
            return text;
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            if (isHex(old) && *whole >= 0) {
                const auto digits = old.substr(2);
                const bool lower = std::ranges::any_of(digits, [](const char c) {
                    return std::islower(static_cast<unsigned char>(c));
                });
                return std::format("{}{}", old.substr(0, 2), lower ? std::format("{:0{}x}", *whole, digits.size()) : std::format("{:0{}X}", *whole, digits.size()));
            }
            return std::format("{}", *whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            auto text = std::format("{}", *number);
            return text.find_first_of(".e") == std::string::npos ? text + ".0" : text;
        }
        return std::get<std::string>(value);
    }

    /**
     * Whether a value is the one written. Two numbers are, when they are as near as a number and its text.
     */
    bool isWritten(const model::Value& value, const model::Value& written)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* writtenNumber = std::get_if<float>(&written);
        return number && writtenNumber ? common::fEqual(*number, *writtenNumber) : value == written;
    }

    /**
     * The type a value is of.
     */
    model::StoredType typeOf(const model::Value& value)
    {
        if (std::holds_alternative<bool>(value)) {
            return model::StoredType::Bool;
        }
        if (std::holds_alternative<int>(value)) {
            return model::StoredType::Int;
        }
        return std::holds_alternative<float>(value) ? model::StoredType::Float : model::StoredType::String;
    }

    /**
     * The line break of a text: the one of its first line, and `\r\n` where it has none, as a file of Windows.
     */
    std::string_view lineBreakOf(const std::string_view text)
    {
        const auto lineBreak = text.find('\n');
        return lineBreak != std::string_view::npos && (lineBreak == 0 || text[lineBreak - 1] != '\r') ? "\n" : "\r\n";
    }
}

namespace mods_config::spec
{
    /**
     * Takes the values of a text as the file's. Any text is an INI file: a line that is no section and no value
     * is left out.
     */
    std::expected<void, std::string> IniFile::load(const std::string& text)
    {
        _text = text;
        _lines = parse(_text);
        return {};
    }

    /**
     * Whether the file has a value of a key, by its section and its key, of any type, as it was last read or
     * written.
     */
    bool IniFile::contains(const std::span<const std::string> path) const
    {
        return find(_lines, path) != nullptr;
    }

    /**
     * The value of a key, by its section and its key, `["Main", "fScale"]`, in a type, as the file was last read
     * or written. A key the file does not have, or whose value is not of the type, is not one, and the reason is
     * returned. A bool is `1`, `true`, `yes` or `on`, and `0`, `false`, `no` or `off`, in any case, and a whole
     * number is in decimals or in hex after `0x`.
     */
    std::expected<model::Value, std::string> IniFile::value(const std::span<const std::string> path, const model::StoredType type) const
    {
        return valueIn(_text, _lines, path, type);
    }

    /**
     * A text with values set in it: each replaces the text of each line of its key, and a key the text does not
     * have is added after the last value of its section, or in a new section at the text's end. The text is
     * checked to read back as the values. Returns the reason for a value that cannot be set.
     */
    std::expected<std::string, std::string> IniFile::edit(const std::string& text, const std::span<const Change> changes) const
    {
        auto edited = text;
        for (const auto& change : changes) {
            if (!keyOf(change.path)) {
                return std::unexpected(std::format("{} is no key of {}, which has a section and a key", pathName(change.path), name()));
            }
            set(edited, change.path, change.value);
        }
        const auto lines = parse(edited);
        for (const auto& change : changes) {
            const auto written = valueIn(edited, lines, change.path, typeOf(change.value));
            if (!written || !isWritten(*written, change.value)) {
                return std::unexpected(std::format("{} cannot be written as \"{}\" in {}", pathName(change.path), valueText(change.value, {}), name()));
            }
        }
        return edited;
    }

    /**
     * A path as a reason names it, as a key of its section: `fScale of [Main]`, and a key before the first
     * section by its name alone.
     */
    std::string IniFile::pathName(const std::span<const std::string> path) const
    {
        if (const auto key = keyOf(path)) {
            return key->first.empty() ? std::string(key->second) : std::format("{} of [{}]", key->second, key->first);
        }
        return path | std::views::join_with('.') | std::ranges::to<std::string>();
    }

    /**
     * The lines of a text. A line whose first character, after its spaces, is `[` and that has a `]` starts the
     * section named between them. A line that starts with `;` or `#` is a comment, and any other line with an
     * `=` and a key before it is a value, all of the text after the `=`. A line break is `\r\n` or `\n`.
     */
    std::vector<IniFile::Line> IniFile::parse(const std::string_view text)
    {
        std::vector<Line> lines;
        std::string section;
        std::size_t begin = text.starts_with(BOM) ? BOM.size() : 0;
        while (begin < text.size()) {
            Line line;
            const auto lineBreak = text.find('\n', begin);
            line.hasBreak = lineBreak != std::string_view::npos;
            line.next = line.hasBreak ? lineBreak + 1 : text.size();
            auto end = line.hasBreak ? lineBreak : text.size();
            if (end > begin && text[end - 1] == '\r') {
                --end;
            }
            const auto content = text.substr(begin, end - begin);
            const auto first = content.find_first_not_of(SPACES);
            const auto equals = content.find('=');
            if (first != std::string_view::npos && content[first] == '[' && content.find(']', first) != std::string_view::npos) {
                line.kind = Line::Kind::Section;
                section = trimmed(content.substr(first + 1, content.find(']', first) - first - 1));
            } else if (first != std::string_view::npos && content[first] != ';' && content[first] != '#' && equals != std::string_view::npos && first < equals) {
                line.kind = Line::Kind::Value;
                line.key = trimmed(content.substr(0, equals));
                const auto keyEnd = content.find_last_not_of(SPACES, equals - 1) + 1;
                const auto valueEnd = content.find_last_not_of(SPACES) + 1;
                const auto valueBegin = valueEnd > equals + 1 ? content.find_first_not_of(SPACES, equals + 1) : equals + 1;
                line.separator = content.substr(keyEnd, valueBegin - keyEnd);
                line.valueBegin = begin + valueBegin;
                line.valueEnd = begin + valueEnd;
            }
            line.section = section;
            lines.push_back(std::move(line));
            begin = lines.back().next;
        }
        return lines;
    }

    /**
     * The last line of the value of a key, by its section and its key. None where the text has none.
     */
    const IniFile::Line* IniFile::find(const std::vector<Line>& lines, const std::span<const std::string> path)
    {
        const auto key = keyOf(path);
        if (!key) {
            return nullptr;
        }
        for (const auto& line : lines | std::views::reverse) {
            if (line.kind == Line::Kind::Value && sameName(line.section, key->first) && sameName(line.key, key->second)) {
                return &line;
            }
        }
        return nullptr;
    }

    /**
     * Sets a value of a key in a text: in place of the value of each line of the key that has another value, so
     * a text of the same value stays, as `1.250` for 1.25, or else in a new line after the last value of its
     * section, or after its last heading. A key of a section the text does not have is added in a new section
     * at its end, after a blank line, and a key before the first section at its start. A new line has the `=`
     * of the line before it, or of the text's first value, and the text's line break.
     */
    void IniFile::set(std::string& text, const std::span<const std::string> path, const model::Value& value)
    {
        const auto [section, key] = *keyOf(path);
        const auto lines = parse(text);
        bool found = false;
        for (const auto& line : lines | std::views::reverse) {
            if (line.kind == Line::Kind::Value && sameName(line.section, section) && sameName(line.key, key)) {
                const auto old = std::string_view(text).substr(line.valueBegin, line.valueEnd - line.valueBegin);
                bool tooLarge = false;
                if (const auto oldValue = valueOf(old, typeOf(value), tooLarge); !oldValue || !isWritten(*oldValue, value)) {
                    text.replace(line.valueBegin, line.valueEnd - line.valueBegin, valueText(value, old));
                }
                found = true;
            }
        }
        if (found) {
            return;
        }

        const Line* after = nullptr;
        const Line* heading = nullptr;
        for (const auto& line : lines) {
            if (sameName(line.section, section) && line.kind == Line::Kind::Value) {
                after = &line;
            } else if (line.kind == Line::Kind::Section && sameName(line.section, section)) {
                heading = &line;
            }
        }
        const auto firstValue = std::ranges::find(lines, Line::Kind::Value, &Line::kind);
        const auto separator = after ? std::string_view(after->separator) : firstValue != lines.end() ? std::string_view(firstValue->separator) : SEPARATOR;
        const auto lineBreak = lineBreakOf(text);
        const auto newLine = std::format("{}{}{}", key, separator, valueText(value, {}));

        if (const auto* line = after ? after : heading) {
            text.insert(line->next, line->hasBreak ? std::format("{}{}", newLine, lineBreak) : std::format("{}{}", lineBreak, newLine));
        } else if (section.empty()) {
            text.insert(text.starts_with(BOM) ? BOM.size() : 0, std::format("{}{}", newLine, lineBreak));
        } else {
            const auto start = text.empty() || text == BOM ? std::string() : text.ends_with('\n') ? std::string(lineBreak) : std::format("{}{}", lineBreak, lineBreak);
            text += std::format("{}[{}]{}{}{}", start, section, lineBreak, newLine, lineBreak);
        }
    }

    /**
     * The value of a key in a text, by its section and its key, in a type, or the reason it is not one.
     */
    std::expected<model::Value, std::string> IniFile::valueIn(const std::string_view text, const std::vector<Line>& lines, const std::span<const std::string> path,
        const model::StoredType type) const
    {
        const auto* line = find(lines, path);
        if (!line) {
            return std::unexpected(std::format("{} is not in {}", pathName(path), name()));
        }
        const auto shown = text.substr(line->valueBegin, line->valueEnd - line->valueBegin);
        bool tooLarge = false;
        auto value = valueOf(shown, type, tooLarge);
        if (!value) {
            const auto what = tooLarge ? "which is too large"s : std::format("which is not {}", typeName(type));
            return std::unexpected(std::format("{} is \"{}\" in {}, {}", pathName(path), shown, name(), what));
        }
        return std::move(*value);
    }
}
