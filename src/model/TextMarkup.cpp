#include "TextMarkup.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <iterator>
#include <optional>

using namespace mods_config;

namespace
{
    // the longest entity that is looked for after a `&`, with its `;`
    constexpr std::size_t MAX_ENTITY_LENGTH = 10;

    using EntityText = std::pair<std::string_view, std::string_view>;

    constexpr std::array ENTITIES{
        EntityText{ "amp", "&" },
        EntityText{ "lt", "<" },
        EntityText{ "gt", ">" },
        EntityText{ "quot", "\"" },
        EntityText{ "apos", "'" },
        EntityText{ "nbsp", " " },
    };

    bool isSpace(const char c)
    {
        return c == ' ' || (c >= '\t' && c <= '\r');
    }

    bool isLetter(const char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    std::string toLower(const std::string_view text)
    {
        std::string lower(text);
        for (auto& c : lower) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        return lower;
    }

    struct Tag
    {
        // in small letters
        std::string name;
        bool closing = false;
        std::string_view attributes;

        // how much of the text the tag is, with its brackets
        std::size_t length = 0;
    };

    /**
     * The tag a text starts with. A `<` that no name follows, or that is not closed, is not one.
     */
    std::optional<Tag> readTag(const std::string_view text)
    {
        const auto end = text.find('>');
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        auto inside = text.substr(1, end - 1);
        while (!inside.empty() && isSpace(inside.front())) {
            inside.remove_prefix(1);
        }
        const bool closing = inside.starts_with('/');
        if (closing) {
            inside.remove_prefix(1);
        }
        std::size_t nameLength = 0;
        while (nameLength < inside.size() && isLetter(inside[nameLength])) {
            ++nameLength;
        }
        if (nameLength == 0) {
            return std::nullopt;
        }
        return Tag{ .name = toLower(inside.substr(0, nameLength)), .closing = closing, .attributes = inside.substr(nameLength), .length = end + 1 };
    }

    /**
     * The value of an attribute of a tag, with or without quotes around it.
     */
    std::optional<std::string_view> findAttribute(std::string_view attributes, const std::string_view name)
    {
        const auto skipSpaces = [&attributes] {
            while (!attributes.empty() && isSpace(attributes.front())) {
                attributes.remove_prefix(1);
            }
        };
        const auto take = [&attributes](const std::size_t length) {
            const auto taken = attributes.substr(0, length);
            attributes.remove_prefix((std::min)(length, attributes.size()));
            return taken;
        };

        while (true) {
            skipSpaces();
            if (attributes.empty()) {
                return std::nullopt;
            }
            std::size_t nameLength = 0;
            while (nameLength < attributes.size() && attributes[nameLength] != '=' && !isSpace(attributes[nameLength])) {
                ++nameLength;
            }
            const auto found = toLower(take(nameLength));
            skipSpaces();
            if (!attributes.starts_with('=')) {
                // a name with no value, or the `/` of a tag that closes itself
                continue;
            }
            attributes.remove_prefix(1);
            skipSpaces();

            std::string_view value;
            if (attributes.starts_with('\'') || attributes.starts_with('"')) {
                const auto quote = attributes.front();
                attributes.remove_prefix(1);
                value = take(attributes.find(quote));
                take(1);
            } else {
                std::size_t valueLength = 0;
                while (valueLength < attributes.size() && !isSpace(attributes[valueLength])) {
                    ++valueLength;
                }
                value = take(valueLength);
            }
            if (found == name) {
                return value;
            }
        }
    }

    /**
     * The size of a font tag, which is a number above zero.
     */
    std::optional<int> toSize(const std::string_view value)
    {
        int size = 0;
        return std::from_chars(value.data(), value.data() + value.size(), size).ec == std::errc() && size > 0 ? std::optional(size) : std::nullopt;
    }

    /**
     * A color that is written as `#RRGGBB`.
     */
    std::optional<std::uint32_t> toColor(const std::string_view value)
    {
        std::uint32_t color = 0;
        if (value.size() != 7 || value.front() != '#') {
            return std::nullopt;
        }
        const auto [end, error] = std::from_chars(value.data() + 1, value.data() + value.size(), color, 16);
        return error == std::errc() && end == value.data() + value.size() ? std::optional(color) : std::nullopt;
    }

    model::TextAlign toAlign(const std::string_view value)
    {
        const auto align = toLower(value);
        if (align == "center") {
            return model::TextAlign::Center;
        }
        return align == "right" ? model::TextAlign::Right : model::TextAlign::Left;
    }

    /**
     * A character by its number, as its bytes in UTF-8. Empty for a number that is no character.
     */
    std::string toUtf8(const std::uint32_t code)
    {
        std::string text;
        if (code == 0 || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
            return text;
        }
        if (code < 0x80) {
            text += static_cast<char>(code);
        } else if (code < 0x800) {
            text += static_cast<char>(0xC0 | (code >> 6));
            text += static_cast<char>(0x80 | (code & 0x3F));
        } else if (code < 0x10000) {
            text += static_cast<char>(0xE0 | (code >> 12));
            text += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (code & 0x3F));
        } else {
            text += static_cast<char>(0xF0 | (code >> 18));
            text += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
            text += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            text += static_cast<char>(0x80 | (code & 0x3F));
        }
        return text;
    }

    struct Entity
    {
        std::string text;

        // how much of the text the entity is, with its `&` and its `;`
        std::size_t length = 0;
    };

    /**
     * The entity a text starts with, as `&amp;` or `&#169;`. A `&` that is not one stays as it is.
     */
    std::optional<Entity> readEntity(const std::string_view text)
    {
        const auto end = text.substr(0, MAX_ENTITY_LENGTH).find(';');
        if (end == std::string_view::npos) {
            return std::nullopt;
        }
        const auto name = text.substr(1, end - 1);
        if (name.starts_with('#')) {
            const bool hex = name.size() > 1 && (name[1] == 'x' || name[1] == 'X');
            const auto digits = name.substr(hex ? 2 : 1);
            std::uint32_t code = 0;
            const auto [last, error] = std::from_chars(digits.data(), digits.data() + digits.size(), code, hex ? 16 : 10);
            auto character = error == std::errc() && last == digits.data() + digits.size() ? toUtf8(code) : std::string();
            return character.empty() ? std::nullopt : std::optional(Entity{ .text = std::move(character), .length = end + 1 });
        }
        const auto found = std::ranges::find(ENTITIES, name, &EntityText::first);
        return found != ENTITIES.end() ? std::optional(Entity{ .text = std::string(found->second), .length = end + 1 }) : std::nullopt;
    }

    struct Style
    {
        std::optional<int> size;
        std::optional<std::uint32_t> color;
    };

    /**
     * The lines of a text as they are read: what is added goes to the line that is open, in the style of the
     * innermost font tag, and in the faces of the `<b>`, `<i>` and `<u>` that are open.
     */
    class Lines
    {
    public:
        void addText(const std::string_view text)
        {
            if (text.empty()) {
                return;
            }
            const auto& style = _styles.empty() ? Style() : _styles.back();
            model::TextRun run{ .text = std::string(text), .size = style.size, .color = style.color, .bold = _bold > 0, .italic = _italic > 0, .underline = _underline > 0 };
            if (!_line.runs.empty() && sameLook(_line.runs.back(), run)) {
                _line.runs.back().text += run.text;
            } else {
                _line.runs.push_back(std::move(run));
            }
        }

        void breakLine()
        {
            _lines.push_back(std::move(_line));
            _line = { .align = _align, .runs = {} };
        }

        /**
         * A paragraph is on lines of its own: the line that is open ends, unless it is empty.
         */
        void setParagraph(const model::TextAlign align)
        {
            if (!_line.runs.empty()) {
                breakLine();
            }
            _align = align;
            _line.align = align;
        }

        void pushStyle(const std::string_view attributes)
        {
            auto style = _styles.empty() ? Style() : _styles.back();
            if (const auto size = findAttribute(attributes, "size")) {
                if (const auto parsed = toSize(*size)) {
                    style.size = parsed;
                }
            }
            if (const auto color = findAttribute(attributes, "color")) {
                if (const auto parsed = toColor(*color)) {
                    style.color = parsed;
                }
            }
            _styles.push_back(style);
        }

        void popStyle()
        {
            if (!_styles.empty()) {
                _styles.pop_back();
            }
        }

        /**
         * Opens or closes a `<b>`, an `<i>` or a `<u>`. Each holds until it closes, whatever font tags open and
         * close in between. A closing tag with none open does nothing.
         */
        void setFace(const std::string_view name, const bool closing)
        {
            auto& open = name == "b" ? _bold : name == "i" ? _italic : _underline;
            open = closing ? (std::max)(open - 1, 0) : open + 1;
        }

        /**
         * The lines that were read, without the empty ones at their end.
         */
        std::vector<model::TextLine> take()
        {
            _lines.push_back(std::move(_line));
            while (!_lines.empty() && _lines.back().runs.empty()) {
                _lines.pop_back();
            }
            return std::move(_lines);
        }

    private:
        std::vector<model::TextLine> _lines;
        model::TextLine _line;
        model::TextAlign _align = model::TextAlign::Left;
        std::vector<Style> _styles;

        // how many of each are open
        int _bold = 0;
        int _italic = 0;
        int _underline = 0;

        static bool sameLook(const model::TextRun& a, const model::TextRun& b)
        {
            return a.size == b.size && a.color == b.color && a.bold == b.bold && a.italic == b.italic && a.underline == b.underline;
        }
    };

    /**
     * Reads a text into lines, at its line ends, and with tags also at its `<br>` and around its paragraphs.
     */
    std::vector<model::TextLine> readLines(const std::string_view text, const bool tags)
    {
        Lines lines;
        std::size_t start = 0;
        const auto addTextTo = [&](const std::size_t end) {
            lines.addText(text.substr(start, end - start));
        };

        for (std::size_t at = 0; at < text.size();) {
            const char c = text[at];
            std::size_t length = 1;
            if (c == '\n' || c == '\r') {
                addTextTo(at);
                if (c == '\n') {
                    lines.breakLine();
                }
            } else if (const auto tag = tags && c == '<' ? readTag(text.substr(at)) : std::nullopt) {
                addTextTo(at);
                length = tag->length;
                if (tag->name == "br") {
                    lines.breakLine();
                } else if (tag->name == "p") {
                    lines.setParagraph(tag->closing ? model::TextAlign::Left : toAlign(findAttribute(tag->attributes, "align").value_or(std::string_view())));
                } else if (tag->name == "font" && tag->closing) {
                    lines.popStyle();
                } else if (tag->name == "font") {
                    lines.pushStyle(tag->attributes);
                } else if (tag->name == "b" || tag->name == "i" || tag->name == "u") {
                    lines.setFace(tag->name, tag->closing);
                }
            } else if (const auto entity = tags && c == '&' ? readEntity(text.substr(at)) : std::nullopt) {
                addTextTo(at);
                length = entity->length;
                lines.addText(entity->text);
            } else {
                ++at;
                continue;
            }
            at += length;
            start = at;
        }
        addTextTo(text.size());
        return lines.take();
    }
}

namespace mods_config::model
{
    /**
     * The lines of a text with tags: `<br>` starts a line, `<p>` is a paragraph with its `align`, `<font>`
     * gives its text a `size` and a `color`, and `<b>`, `<i>` and `<u>` make it bold, italic and underlined.
     * Any other tag is dropped and its text is kept.
     * Entities, as `&amp;`, are decoded.
     */
    std::vector<TextLine> parseMarkup(const std::string_view text)
    {
        return readLines(text, true);
    }

    /**
     * The lines of a text that takes no tags: what looks like one is shown as it is written.
     */
    std::vector<TextLine> plainLines(const std::string_view text)
    {
        return readLines(text, false);
    }

    /**
     * The runs of a text with tags, on one line: a line break and the end of a paragraph become a space, and
     * an empty line is dropped. A run keeps the size and the color of its font tag, and its faces.
     */
    std::vector<TextRun> markupRuns(const std::string_view text)
    {
        std::vector<TextRun> runs;
        for (auto& line : parseMarkup(text)) {
            if (std::ranges::all_of(line.runs, [](const TextRun& run) {
                    return run.text.find_first_not_of(' ') == std::string::npos;
                })) {
                continue;
            }
            if (!runs.empty()) {
                runs.push_back({ .text = " ", .size = {}, .color = {} });
            }
            std::ranges::move(line.runs, std::back_inserter(runs));
        }
        return runs;
    }

    /**
     * A text without its tags, on one line: a line break and the end of a paragraph become a space. The spaces
     * the text starts with stay, as FallUI sets a row in by them.
     */
    std::string removeMarkup(const std::string_view text)
    {
        std::string plain;
        for (const auto& line : parseMarkup(text)) {
            std::string words;
            for (const auto& run : line.runs) {
                words += run.text;
            }
            const auto first = words.find_first_not_of(' ');
            if (first == std::string::npos) {
                continue;
            }
            const auto start = plain.empty() ? 0 : first;
            if (!plain.empty()) {
                plain += ' ';
            }
            plain += words.substr(start, words.find_last_not_of(' ') + 1 - start);
        }
        return plain;
    }
}
