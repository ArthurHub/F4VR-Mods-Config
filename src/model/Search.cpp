#include "Search.h"

#include <algorithm>
#include <type_traits>
#include <variant>

#include "Conditions.h"

namespace
{
    using namespace mods_config::model;

    // the letter each character of Latin-1's upper half folds to, from U+00C0: `*` for a letter with no other,
    // which is only put in lower case, and `-` for the two signs that are no letter
    constexpr std::string_view LATIN_1 = "aaaaaa*ceeeeiiii*nooooo-ouuuuy**aaaaaa*ceeeeiiii*nooooo-ouuuuy*y";

    // the same of Latin Extended-A, from U+0100: a `*` there is a pair of an upper case and a lower case letter
    constexpr std::string_view LATIN_EXTENDED_A =
        "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiii**jjkkkllllllllllnnnnnnnnnoooooo**rrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
    static_assert(LATIN_1.size() == 0x40 && LATIN_EXTENDED_A.size() == 0x80);

    /**
     * The character of UTF-8 at a place of a text, and moves the place past it. A byte that starts no character,
     * and a character that is cut short, are U+FFFD, one byte each.
     */
    char32_t decode(const std::string_view text, std::size_t& place)
    {
        const auto lead = static_cast<unsigned char>(text[place++]);
        if (lead < 0x80) {
            return lead;
        }
        const int more = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : -1;
        if (more < 0 || place + more > text.size()) {
            return U'�';
        }
        char32_t c = lead & (0x3F >> more);
        for (int i = 0; i < more; ++i) {
            const auto next = static_cast<unsigned char>(text[place]);
            if ((next & 0xC0) != 0x80) {
                return U'�';
            }
            c = (c << 6) | (next & 0x3F);
            ++place;
        }
        return c;
    }

    /**
     * Whether a character splits the words of a name: a space, a punctuation mark or a sign. Every other character
     * of a script is a letter.
     */
    bool isSeparator(const char32_t c)
    {
        if (c < 0x80) {
            return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
        }
        return (c >= 0xA0 && c <= 0xBF) || c == 0xD7 || c == 0xF7 || (c >= 0x2000 && c <= 0x206F) || (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF01 && c <= 0xFF0F) ||
               (c >= 0xFF1A && c <= 0xFF20) || (c >= 0xFF3B && c <= 0xFF40) || (c >= 0xFF5B && c <= 0xFF65);
    }

    /**
     * Whether a character is of a script that writes no space between its words: Chinese, Japanese and Korean.
     */
    bool isCjk(const char32_t c)
    {
        return (c >= 0x3040 && c <= 0x30FF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0x4E00 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7AF) || (c >= 0xF900 && c <= 0xFAFF) ||
               (c >= 0xFF66 && c <= 0xFF9F);
    }

    /**
     * A character in lower case and without its accent, for Latin, Greek and Cyrillic: one character for one, so a
     * match in the folded name is a match of the same characters of the name. Any other character is as it is.
     */
    char32_t foldChar(const char32_t c)
    {
        if (c < 0x80) {
            return c >= 'A' && c <= 'Z' ? c + 0x20 : c;
        }
        if (c >= 0xC0 && c <= 0xFF) {
            const char letter = LATIN_1[c - 0xC0];
            if (letter == '-') {
                return c;
            }
            // ß is in the upper half and has no upper case
            return letter == '*' ? (c < 0xDF ? c + 0x20 : c) : static_cast<char32_t>(letter);
        }
        if (c >= 0x100 && c <= 0x17F) {
            const char letter = LATIN_EXTENDED_A[c - 0x100];
            return letter == '*' ? (c | 1) : static_cast<char32_t>(letter);
        }
        if (c >= 0x218 && c <= 0x21B) {
            // Romanian's s and t with a comma
            return c < 0x21A ? U's' : U't';
        }
        switch (c) {
        case 0x386:
        case 0x3AC:
            return 0x3B1;
        case 0x388:
        case 0x3AD:
            return 0x3B5;
        case 0x389:
        case 0x3AE:
            return 0x3B7;
        case 0x38A:
        case 0x3AA:
        case 0x3AF:
        case 0x390:
        case 0x3CA:
            return 0x3B9;
        case 0x38C:
        case 0x3CC:
            return 0x3BF;
        case 0x38E:
        case 0x3AB:
        case 0x3B0:
        case 0x3CB:
        case 0x3CD:
            return 0x3C5;
        case 0x38F:
        case 0x3CE:
            return 0x3C9;
        case 0x3C2:
            // the final sigma
            return 0x3C3;
        case 0x401:
        case 0x451:
            // Ё and ё are searched as Е
            return 0x435;
        default:
            break;
        }
        if (c >= 0x391 && c <= 0x3A9) {
            return c + 0x20;
        }
        if (c >= 0x410 && c <= 0x42F) {
            return c + 0x20;
        }
        if (c >= 0x400 && c <= 0x40F) {
            return c + 0x50;
        }
        return c;
    }

    /**
     * A name as the search matches it: its characters folded, each separator a space, with the byte of the name each
     * starts at and then the name's size. An accent that is a character of its own is left out, so it belongs to
     * the character before it.
     */
    struct Folded
    {
        std::u32string text;
        std::vector<std::size_t> bytes;
    };

    Folded fold(const std::string_view name)
    {
        Folded folded;
        std::size_t place = 0;
        while (place < name.size()) {
            const auto start = place;
            const char32_t c = decode(name, place);
            if (c >= 0x300 && c <= 0x36F) {
                continue;
            }
            folded.text.push_back(isSeparator(c) ? U' ' : foldChar(c));
            folded.bytes.push_back(start);
        }
        folded.bytes.push_back(name.size());
        return folded;
    }

    /**
     * Where a word of the query matches a name: at the start of a word of the name that it starts, or, for a word of
     * a script with no spaces, anywhere in the name. None where it does not.
     */
    std::optional<std::size_t> find(const Folded& name, const std::u32string& word)
    {
        if (std::ranges::any_of(word, isCjk)) {
            const auto at = name.text.find(word);
            return at == std::u32string::npos ? std::nullopt : std::optional(at);
        }
        for (std::size_t at = 0; at + word.size() <= name.text.size(); ++at) {
            if ((at == 0 || name.text[at - 1] == U' ') && name.text.compare(at, word.size(), word) == 0) {
                return at;
            }
        }
        return std::nullopt;
    }

    /**
     * Whether a name matches the query: every word of the query matches the name or a name it is under, as its mod's,
     * its page's or its section's, and at least one matches the name itself.
     */
    bool matches(const SearchQuery& query, const Folded& name, const std::vector<Folded>& context)
    {
        bool inName = false;
        for (const auto& word : query.words()) {
            if (find(name, word)) {
                inName = true;
            } else if (std::ranges::none_of(context, [&word](const Folded& above) {
                           return find(above, word).has_value();
                       })) {
                return false;
            }
        }
        return inName;
    }

    /**
     * The search of one mod, with the names of the mod, the pages and the sections it is in as it goes down.
     * The most specific match wins: a section, a page or a mod whose name matches shows all that is under it only
     * when nothing under it matches.
     */
    class ModSearch
    {
    public:
        ModSearch(const SearchQuery& query, Store* store)
            : _query(query),
              _store(store)
        {}

        bool matches(const Folded& name) const
        {
            return ::matches(_query, name, _context);
        }

        /**
         * Fills what the search shows of a mod: the rows of its front page and its pages, those that match or, with
         * `all`, every one, and the items of the rows and of the sections they are in.
         */
        void fill(ModFound& found, const Mod& mod, const Folded& name, const bool all)
        {
            _rows = &found.rows;
            _context.push_back(name);
            found.frontCount = countItems(mod.frontPage.items, all);
            found.pages = searchPages(mod.pages, all);
            _context.pop_back();
        }

    private:
        /**
         * The pages of a list the search shows, in their order.
         */
        std::vector<PageFound> searchPages(const std::vector<Page>& pages, const bool all)
        {
            std::vector<PageFound> found;
            for (std::size_t place = 0; place < pages.size(); ++place) {
                if (auto page = searchPage(pages[place], place, all)) {
                    found.push_back(std::move(*page));
                }
            }
            return found;
        }

        /**
         * A page the search shows: one with a row that matches, or with a page under it that it shows, or, with
         * nothing that matches under it, one whose name matches, with all its rows and pages. A page a condition
         * hides is not searched, and a disabled one is shown by its name alone.
         */
        std::optional<PageFound> searchPage(const Page& page, const std::size_t place, const bool all)
        {
            if (page.visibleWhen && !holds(*page.visibleWhen, _store)) {
                return std::nullopt;
            }
            const auto name = fold(page.name);
            const bool own = matches(name);
            const bool enabled = page.notAvailable.empty() && (!page.enabledWhen || holds(*page.enabledWhen, _store));
            if (!enabled) {
                return all || own ? std::optional(PageFound{ .place = place, .count = 0, .enabled = false, .pages = {} }) : std::nullopt;
            }
            _context.push_back(name);
            PageFound found{ .place = place, .count = countItems(page.items, all), .enabled = true, .pages = searchPages(page.pages, all) };
            const bool empty = found.count == 0 && found.pages.empty();
            if (!all && empty && own) {
                found.count = countItems(page.items, true);
                found.pages = searchPages(page.pages, true);
            }
            _context.pop_back();
            if (!all && empty && !own) {
                return std::nullopt;
            }
            return found;
        }

        /**
         * How many rows of a list of items, and of the sections in it, the search shows: a setting, a button, a
         * hotkey and a control the menu does not have, that a condition does not hide and that match, or every one
         * with `all`. Each row it shows is kept, and so is a section with one.
         */
        std::size_t countItems(const std::vector<Item>& items, const bool all)
        {
            std::size_t count = 0;
            for (const auto& item : items) {
                if (const auto* group = std::get_if<Group>(&item)) {
                    const auto rows = countGroup(*group, all);
                    if (rows > 0) {
                        _rows->insert(&item);
                    }
                    count += rows;
                    continue;
                }
                const bool shown = std::visit(
                    [this, all]<typename T>(const T& row) {
                        if constexpr (std::is_same_v<T, Setting>) {
                            // a row the menu makes is shown with its section or its page, and not found by its own text
                            return (!row.visibleWhen || holds(*row.visibleWhen, _store)) && (all || (!row.notSearched && matches(fold(row.text))));
                        } else if constexpr (std::is_same_v<T, Button> || std::is_same_v<T, Hotkey> || std::is_same_v<T, Placeholder>) {
                            return (!row.visibleWhen || holds(*row.visibleWhen, _store)) && (all || matches(fold(row.text)));
                        } else {
                            return false;
                        }
                    },
                    item);
                if (shown) {
                    _rows->insert(&item);
                    ++count;
                }
            }
            return count;
        }

        /**
         * How many rows of a section the search shows: those that match, or all of them when none does and its
         * heading does. A condition on a section hides its heading alone, so its rows are searched either way.
         */
        std::size_t countGroup(const Group& group, const bool all)
        {
            if (all) {
                return countItems(group.items, true);
            }
            const auto heading = fold(group.text);
            const bool own = !group.text.empty() && (!group.visibleWhen || holds(*group.visibleWhen, _store)) && matches(heading);
            _context.push_back(heading);
            auto count = countItems(group.items, false);
            if (count == 0 && own) {
                count = countItems(group.items, true);
            }
            _context.pop_back();
            return count;
        }

        const SearchQuery& _query;
        Store* _store;

        // the names the item searched now is under: its mod's, its pages' and its sections'
        std::vector<Folded> _context;

        // where the items the search shows are kept: those of the mod it fills
        std::unordered_set<const Item*>* _rows = nullptr;
    };

    std::size_t totalOf(const std::vector<PageFound>& pages)
    {
        std::size_t total = 0;
        for (const auto& page : pages) {
            total += page.count + totalOf(page.pages);
        }
        return total;
    }
}

namespace mods_config::model
{
    /**
     * The words of a query, folded as a name is, split at its spaces and its punctuation.
     */
    SearchQuery::SearchQuery(const std::string_view text)
    {
        const auto folded = fold(text);
        std::u32string word;
        for (const char32_t c : folded.text) {
            if (c != U' ') {
                word.push_back(c);
            } else if (!word.empty()) {
                _words.push_back(std::move(word));
                word.clear();
            }
        }
        if (!word.empty()) {
            _words.push_back(std::move(word));
        }
    }

    /**
     * Where the words of a query match a name, as its bytes, in their order: each word at the start of the first
     * word of the name it starts, or where it is in the name for a word of Chinese, Japanese or Korean. A word that
     * matches only a name the item is under has no range in it.
     */
    std::vector<TextRange> matchRanges(const SearchQuery& query, const std::string_view name)
    {
        const auto folded = fold(name);
        std::vector<TextRange> ranges;
        for (const auto& word : query.words()) {
            if (const auto at = find(folded, word)) {
                ranges.push_back({ .begin = folded.bytes[*at], .end = folded.bytes[*at + word.size()] });
            }
        }
        std::ranges::sort(ranges, {}, &TextRange::begin);
        return ranges;
    }

    /**
     * What a query finds in a mod: the rows that match, of its front page and of its pages, and the pages they are
     * on, or, with nothing under it that matches, every page of a mod whose name matches. A mod that is not
     * available is found by its name alone. The items it shows are kept, so its page shows those alone. With no store, as for a mod that is not selected, a condition holds, so
     * a row it would hide is found. None for a mod in which nothing matches.
     */
    std::optional<ModFound> searchMod(const Mod& mod, const SearchQuery& query, Store* store)
    {
        if (query.empty()) {
            return std::nullopt;
        }
        ModSearch search(query, store);
        const auto name = fold(mod.displayName);
        const bool own = search.matches(name);
        if (!mod.notAvailable.empty()) {
            return own ? std::optional(ModFound{ .frontCount = 0, .pages = {}, .byName = true, .total = 0 }) : std::nullopt;
        }
        ModFound found;
        search.fill(found, mod, name, false);
        if (found.frontCount == 0 && found.pages.empty()) {
            if (!own) {
                return std::nullopt;
            }
            found.byName = true;
            search.fill(found, mod, name, true);
        }
        found.total = found.frontCount + totalOf(found.pages);
        return found;
    }
}
