/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <utility>
# include <fast_io.h>
# include <fast_io_dsal/string.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include "console_help_catalog.h"
# include "console_aliases.h"
#endif
#include <uwvm2/utils/ansies/ansi_push_macro.h>
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_help
{
    // Text comes only from the static catalogue. Help never queries a guest or
    // evaluates its selector. Use UWVM's diagnostic policy and ANSI palette;
    // the console adapter has already qualified VT output on interactive hosts.
    class writer
    {
        ::fast_io::string text_{};
        ::std::size_t columns_{};
        bool terminal_{};
    public:
        explicit writer(bool terminal, ::std::size_t columns) noexcept
            : columns_{columns == 0u ? 1u : columns > 80u ? 80u : columns}, terminal_{terminal} {}
        template<::std::size_t N> void color(char const (&ansi)[N])
        { if(terminal_) { text_.append(::fast_io::concat_fast_io(::uwvm2::uwvm::utils::ansies::diagnostic_color(ansi))); } }
        template<::std::size_t N> void line(view text, char const (&ansi)[N], ::std::size_t indent = 0u)
        {
            while(!text.empty())
            {
                auto const newline{text.find("\n")};
                auto part{text.subview(0u, newline == ::fast_io::containers::npos ? text.size() : newline)};
                do
                {
                    auto const prefix{indent < columns_ ? indent : 0u};
                    auto const budget{columns_ - prefix};
                    ::std::size_t size{part.size() < budget ? part.size() : budget};
                    if(size < part.size())
                    {
                        auto space{part.subview(0u, size).rfind(" ")};
                        if(space != ::fast_io::containers::npos && space != 0u) { size = space; }
                    }
                    for(::std::size_t i{}; i < prefix; ++i) { text_.push_back(' '); }
                    color(ansi); text_.append(part.subview(0u, size)); color(UWVM_AES_RST_ALL); text_.push_back('\n');
                    part = part.subview(size); while(part.starts_with(" ")) { part = part.subview(1u); }
                } while(!part.empty());
                if(newline == ::fast_io::containers::npos) { break; }
                text = text.subview(newline + 1u);
            }
        }
        void blank() { text_.push_back('\n'); }
        void label(view text) { line(text, UWVM_AES_LT_PURPLE); }
        void paragraph(view text) { line(text, UWVM_AES_WHITE); }
        void row(view command, view description, ::std::size_t width)
        {
            if(command.size() > width || columns_ < width + 27u || description.size() > columns_ - (width + 9u))
            { line(command, UWVM_AES_LT_GREEN, 2u); line(description, UWVM_AES_WHITE, 4u); return; }
            text_.append("  "); color(UWVM_AES_LT_GREEN);
            text_.append(::fast_io::concat_fast_io(::fast_io::mnp::left(command, width)));
            color(UWVM_AES_YELLOW); text_.append("  --   "); color(UWVM_AES_WHITE);
            text_.append(description); color(UWVM_AES_RST_ALL); text_.push_back('\n');
        }
        [[nodiscard]] ::fast_io::string const& text() const noexcept { return text_; }
    };
    [[nodiscard]] constexpr view first_line(view text) noexcept
    { auto const end{text.find("\n")}; return text.subview(0u, end == ::fast_io::containers::npos ? text.size() : end); }
    [[nodiscard]] constexpr view primary_usage(view text) noexcept
    {
        auto end{text.find("\n")}; if(end == ::fast_io::containers::npos) { return text; }
        while(end + 1u < text.size() && text[end + 1u] == ' ')
        { auto next{text.subview(end + 1u).find("\n")}; if(next == ::fast_io::containers::npos) { return text; } end += next + 1u; }
        return text.subview(0u, end);
    }
    [[nodiscard]] inline ::fast_io::string format_error(view text, bool terminal, ::std::size_t columns = 80u)
    {
        writer output{terminal, columns}; auto const first{first_line(text)};
        output.line(first, UWVM_AES_LT_RED);
        if(first.size() < text.size()) { output.paragraph(text.subview(first.size() + 1u)); }
        return ::fast_io::concat_fast_io(output.text());
    }
    [[nodiscard]] constexpr view when(view key) noexcept
    {
        if(key == "language frames bt") { return "Requires a stopped program; source debug information is optional."; }
        if(key == "language values display") { return "Expressions are printed when the program stops."; }
        if(key.starts_with("language")) { return "Requires a stopped program and matching source debug information (-g)."; }
        if(key == "asm registers" || key == "asm ni" || key == "asm finish")
        { return "Requires a native instruction stop; use si from a Wasm stop first."; }
        if(key.starts_with("asm")) { return "Requires a stopped program; only Wasm JIT code is accessible."; }
        if(key.starts_with("wasm events") || key == "wasm script" || key == "wasip1 trace") { return {}; }
        if(key.starts_with("wasm") || key.starts_with("wasip1") || key.starts_with("checkpoints") || key == "update replace")
        { return "Requires the program to be paused. Use status to see its current state."; }
        return {};
    }
    struct request { view key{}; ::std::size_t page{1u}; bool valid{true}, full{}; };
    [[nodiscard]] inline request parse(view selector) noexcept
    {
        request result{selector};
        if(selector.size() > maximum_selector_bytes) { result.valid = false; return result; }
        for(unsigned char c : selector) { if(c < 32u || c > 126u) { result.valid = false; return result; } }
        auto const space{selector.rfind(" ")};
        auto const last{space == ::fast_io::containers::npos ? selector : selector.subview(space + 1u)};
        if(!last.empty() && last.front() >= '0' && last.front() <= '9')
        {
            auto const end{last.data() + last.size()};
            auto const scanned{::fast_io::parse_by_scan(last.data(), end, ::fast_io::mnp::dec_get<true, true>(result.page))};
            if(scanned.code != ::fast_io::parse_code::ok || scanned.iter != end || result.page == 0u)
            { result.valid = false; return result; }
            result.key = space == ::fast_io::containers::npos ? view{} : selector.subview(0u, space);
        }
        constexpr view detail_suffixes[]{" full", " details"};
        for(auto suffix : detail_suffixes)
        { if(result.key.ends_with(suffix)) { result.full = true; result.key = result.key.subview(0u, result.key.size() - suffix.size()); break; } }
        result.key = canonical(result.key); return result;
    }
    // Resolve documentation only. Command normalization never executes, parses
    // expressions, or asks the controller for source/native state.
    struct topic_resolution
    {
        entry const* selected{};
        ::fast_io::array<view, 8u> candidates{};
        ::std::size_t size{};
        bool ambiguous{}, truncated{};
    };
    // One bounded command-word edit only. Numeric command names (wasip1)
    // participate, but expressions, paths and arbitrary input bytes do not.
    [[nodiscard]] inline bool single_typo(view word, view candidate) noexcept
    {
        if(word.size() < 3u || word.size() > 32u || word == candidate ||
           candidate.size() + 1u < word.size() || word.size() + 1u < candidate.size()) { return false; }
        for(char c : word) { if((c < 'a' || c > 'z') && (c < '0' || c > '9') && c != '-') { return false; } }
        if(word.size() == candidate.size())
        {
            ::std::size_t first{}; while(first < word.size() && word[first] == candidate[first]) { ++first; }
            auto rest{first + 1u};
            if(rest < word.size() && word[first] == candidate[rest] && word[rest] == candidate[first]) { ++rest; }
            while(rest < word.size() && word[rest] == candidate[rest]) { ++rest; }
            return rest == word.size();
        }
        auto const shorter{word.size() < candidate.size() ? word : candidate};
        auto const longer{word.size() < candidate.size() ? candidate : word};
        ::std::size_t first{}; while(first < shorter.size() && shorter[first] == longer[first]) { ++first; }
        for(auto i{first}; i < shorter.size(); ++i) { if(shorter[i] != longer[i + 1u]) { return false; } }
        return true;
    }
    [[nodiscard]] inline bool typo_path(view input, view candidate) noexcept
    {
        bool changed{};
        while(!input.empty() && !candidate.empty())
        {
            auto const a{console_aliases::split(input)}, b{console_aliases::split(candidate)};
            if(a.word != b.word)
            { if(changed || !single_typo(a.word, b.word)) { return false; } changed = true; }
            input = a.tail; candidate = b.tail;
        }
        return changed && input.empty() && candidate.empty();
    }
    struct topic_suggestions { ::fast_io::array<view, 3u> names{}; ::std::size_t size{}; };
    [[nodiscard]] inline topic_suggestions suggest_topic(view key)
    {
        topic_suggestions result{};
        if(key.empty() || key.size() > maximum_selector_bytes) { return result; }
        auto const normalized{console_aliases::normalize(key)};
        if(normalized.ambiguous) { return result; }
        ::fast_io::string spelling{};
        if(normalized.valid && !normalized.view().empty()) { key = normalized.view(); }
        else if(!normalized.head.empty())
        {
            auto const tail{console_aliases::split(key).tail};
            spelling = ::fast_io::concat_fast_io(normalized.head, tail.empty() ? view{} : view{" "}, tail);
            key = spelling.subview(0u);
        }
        auto add{[&](view name)
        {
            if(result.size == result.names.size() || !typo_path(key, name)) { return; }
            auto const target{canonical(name)};
            if(find(target) == nullptr) { return; }
            for(::std::size_t i{}; i < result.size; ++i)
            { if(canonical(result.names[i]) == target) { return; } }
            // Retain only static catalogue views. Never retain or echo input.
            result.names[result.size++] = name;
        }};
        for(auto const& alias : aliases) { add(alias.name); }
        for(auto const& item : entries) { add(item.key); }
        return result;
    }
    // These selectors grant documentation access only. Incomplete roots use
    // the SAME resolved command spelling as execution; aliases such as c/r/s
    // must never be interpreted as menu prefixes.
    [[nodiscard]] inline prefix_menu const* menu_request(view input)
    {
        auto const normalized{console_aliases::normalize(input)};
        if(normalized.ambiguous || normalized.head.empty()) { return nullptr; }
        auto const candidate{normalized.valid ? normalized.view() :
            console_aliases::split(input).tail.empty() ? normalized.head : view{}};
        for(auto const& menu : prefix_menus) { if(candidate == menu.command) { return &menu; } }
        return nullptr;
    }
    [[nodiscard]] inline prefix_menu const* menu_context(view input)
    {
        auto const normalized{console_aliases::normalize(input)};
        if(normalized.ambiguous || normalized.head.empty()) { return nullptr; }
        auto const tail{console_aliases::split(input).tail};
        auto const spelling{::fast_io::concat_fast_io(normalized.head, tail.empty() ? view{} : view{" "}, tail)};
        prefix_menu const* selected{};
        for(auto const& menu : prefix_menus)
        {
            if(!spelling.subview(0u).starts_with(menu.command) ||
               (spelling.size() != menu.command.size() && spelling[menu.command.size()] != ' ') ||
               (selected != nullptr && selected->command.size() >= menu.command.size())) { continue; }
            selected = &menu;
        }
        return selected;
    }
    [[nodiscard]] inline topic_resolution resolve_topic(view key)
    {
        topic_resolution result{};
        if(auto const exact{find(canonical(key))}; exact != nullptr) { result.selected = exact; return result; }
        auto const normalized{console_aliases::normalize(key)};
        if(normalized.valid)
        { if(auto const command{find(canonical(normalized.view()))}; command != nullptr) { result.selected = command; return result; } }
        if(!normalized.ambiguous && !normalized.head.empty() && console_aliases::split(key).tail.empty())
        { if(auto const group{find(canonical(normalized.head))}; group != nullptr && group->group()) { result.selected = group; return result; } }
        if(normalized.ambiguous)
        {
            result.ambiguous = true; result.truncated = normalized.candidates.truncated;
            for(::std::size_t i{}; i < normalized.candidates.size; ++i)
            {
                auto const name{normalized.candidates.names[i]};
                // Offer static catalogue labels rather than borrowed normalized
                // input. Scoped command candidates need their original prefix.
                for(auto const& alias : aliases)
                {
                    auto parts{console_aliases::split(alias.name)};
                    if((normalized.candidates.scope.empty() && alias.name == name) ||
                       (parts.word == normalized.candidates.scope && parts.tail == name))
                    {
                        bool seen{}; for(::std::size_t n{}; n < result.size; ++n) { seen = seen || result.candidates[n] == alias.name; }
                        if(!seen && result.size < result.candidates.size()) { result.candidates[result.size++] = alias.name; }
                        break;
                    }
                }
            }
            return result;
        }
        // Category words expand one level at a time; arguments are never
        // interpreted as category paths. Exact child names always win.
        // Keep a recognized command root even when its child is invalid:
        // "i waspi1 args" is in info, not an ambiguous i... category lookup.
        ::fast_io::string spelling{};
        if(!normalized.head.empty())
        {
            auto const tail{console_aliases::split(key).tail};
            spelling = ::fast_io::concat_fast_io(normalized.head, tail.empty() ? view{} : view{" "}, tail);
            key = spelling.subview(0u);
        }
        view parent{};
        while(!key.empty())
        {
            auto const parts{console_aliases::split(key)}; entry const* chosen{};
            result.size = 0u; result.truncated = false; ::std::size_t matches{};
            for(auto const& item : entries)
            {
                if(item.parent != parent) { continue; }
                auto const word{item.key.subview(parent.empty() ? 0u : parent.size() + 1u)};
                if(word == parts.word) { chosen = &item; matches = 1u; result.size = 0u; result.truncated = false; break; }
                if(!word.starts_with(parts.word)) { continue; }
                chosen = &item; ++matches;
                if(result.size < result.candidates.size()) { result.candidates[result.size++] = item.key; }
                else { result.truncated = true; }
            }
            if(matches != 1u) { result.ambiguous = matches > 1u; return result; }
            if(parts.tail.empty()) { result.selected = chosen; return result; }
            if(!chosen->group()) { result.size = 0u; return result; }
            parent = chosen->key; key = parts.tail;
        }
        return result;
    }
    // Both help and keyword discovery page physical output rows, so a narrow
    // terminal never receives an unbounded catalogue dump.
    [[nodiscard]] inline ::fast_io::string paginate(view text, view title, view next_command, ::std::size_t page,
        bool terminal, ::std::size_t columns, ::std::size_t page_rows)
    {
        ::std::size_t lines{}; for(char c : text) { if(c == '\n') { ++lines; } }
        page_rows = page_rows < 4u ? 4u : page_rows > 18u ? 18u : page_rows;
        auto const pages{lines == 0u ? 1u : (lines + page_rows - 1u) / page_rows};
        if(page == 0u || page > pages)
        { writer error{terminal, columns}; error.paragraph(::fast_io::concat_fast_io("Help page out of range; available pages: 1..", ::fast_io::mnp::dec(pages), ".").subview(0u)); return ::fast_io::concat_fast_io(error.text()); }
        ::std::size_t begin{}, end{}, index{}; auto const first{(page - 1u) * page_rows};
        for(::std::size_t i{}; i < text.size(); ++i)
        { if(index == first) { begin = i; break; } if(text[i] == '\n') { ++index; } }
        end = begin; index = 0u;
        while(end < text.size() && index < page_rows) { if(text[end++] == '\n') { ++index; } }
        writer header{terminal, columns}; header.line(title, UWVM_AES_LT_CYAN); header.blank();
        writer footer{terminal, columns};
        auto const label{::fast_io::concat_fast_io("Page ", ::fast_io::mnp::dec(page), "/", ::fast_io::mnp::dec(pages))};
        if(page < pages)
        { footer.line(::fast_io::concat_fast_io(label, "  Next: ", next_command, " ", ::fast_io::mnp::dec(page + 1u)).subview(0u), UWVM_AES_LT_PURPLE); }
        else { footer.line(label.subview(0u), UWVM_AES_LT_PURPLE); }
        return ::fast_io::concat_fast_io(header.text(), text.subview(begin, end - begin), footer.text());
    }
    [[nodiscard]] inline ::fast_io::string format(view selector = {}, bool terminal = false,
        ::std::size_t columns = 80u, ::std::size_t page_rows = 18u)
    {
        auto requested{parse(selector)};
        auto const resolved{requested.valid && !requested.key.empty() ? resolve_topic(requested.key) : topic_resolution{}};
        auto const selected{resolved.selected};
        if(!requested.valid || (!requested.key.empty() && selected == nullptr))
        {
            writer error{terminal, columns}; error.line(resolved.ambiguous ? view{"Ambiguous help topic. Candidates:"} : view{"Unknown help topic or invalid page."}, UWVM_AES_YELLOW);
            for(::std::size_t i{}; i < resolved.size; ++i)
            { error.line(::fast_io::concat_fast_io("help ", resolved.candidates[i]).subview(0u), UWVM_AES_LT_GREEN, 2u); }
            if(resolved.truncated) { error.paragraph("More candidates: narrow the topic or use Tab."); }
            if(requested.valid && !resolved.ambiguous)
            {
                auto const suggestions{suggest_topic(requested.key)};
                if(suggestions.size != 0u) { error.label("Did you mean:"); }
                for(::std::size_t i{}; i < suggestions.size; ++i)
                {
                    auto command{::fast_io::concat_fast_io("help ", suggestions.names[i], requested.full ? view{" full"} : view{})};
                    if(requested.page != 1u) { command.append(::fast_io::concat_fast_io(" ", ::fast_io::mnp::dec(requested.page))); }
                    error.line(command.subview(0u), UWVM_AES_LT_GREEN, 2u);
                }
            }
            error.paragraph("Use help for topics, help COMMAND for usage, or apropos WORD to find a command.");
            return ::fast_io::concat_fast_io(error.text());
        }
        if(selected != nullptr) { requested.key = selected->key; }
        writer body{terminal, columns};
        if(selected == nullptr)
        {
            body.label("Command classes:");
            for(auto const& item : entries)
            {
                if(!item.parent.empty() || !item.group() || item.key == "update" || item.key == "checkpoints" || item.key == "tui" || item.key == "editing") { continue; }
                auto const command{::fast_io::concat_fast_io("help ", item.key)};
                body.row(command.subview(0u), item.summary, 21u);
            }
            body.blank(); body.paragraph("More: help update | help checkpoints | help tui | help editing");
            body.paragraph("Type help COMMAND for a short explanation and example (e.g. help up).");
            body.paragraph("Type help COMMAND full for all arguments. Tab completes commands.");
            body.paragraph("Find a command by purpose: apropos WORD (e.g. apropos register).");
        }
        else
        {
            body.paragraph(selected->summary); body.blank();
            if(selected->group())
            {
                if(selected->key.starts_with("commands "))
                {
                    for(auto const& choice : menu_choices)
                    {
                        if(choice.parent != selected->key) { continue; }
                        if(auto const page{find(choice.topic)}; page != nullptr) { body.row(choice.command, page->summary, 25u); }
                    }
                    body.blank(); body.paragraph("Type a full command, or help COMMAND for its arguments and an example.");
                }
                else
                {
                if(selected->key == "info")
                {
                    body.row("info threads", "Show stopped threads and their labels.", 22u);
                    body.row("info breakpoints", "List breakpoints and their IDs.", 22u);
                    body.row("info locals", "Show source variables in the selected frame.", 22u);
                    body.row("info registers", "Read the current Wasm native registers.", 22u);
                    body.row("info wasm-events", "List Wasm catchpoints.", 22u);
                    body.row("info wasip1", "Choose arguments, env or descriptor metadata.", 22u);
                    body.paragraph("Type help followed by the full command for details.");
                }
                ::std::size_t width{};
                for(auto const& item : entries)
                { if(item.parent == selected->key && item.key.size() + 5u > width) { width = item.key.size() + 5u; } }
                for(auto const& item : entries)
                {
                    if(item.parent != selected->key) { continue; }
                    auto const command{::fast_io::concat_fast_io("help ", item.key)};
                    body.row(command.subview(0u), item.summary, width);
                }
                if(selected->key == "wasm set") { body.paragraph("For guest arguments, env and files: help wasip1"); }
                }
            }
            else if(requested.full)
            {
                body.label("Usage:"); body.line(selected->usage, UWVM_AES_LT_GREEN, 2u);
                body.blank(); body.label("Arguments:"); body.line(selected->arguments, UWVM_AES_WHITE, 2u);
                body.blank(); body.label("Examples:"); body.line(selected->examples, UWVM_AES_YELLOW, 2u);
            }
            else
            {
                body.label("Usage:"); body.line(primary_usage(selected->usage), UWVM_AES_LT_GREEN, 2u);
                auto const requirement{when(selected->key)};
                if(!requirement.empty()) { body.paragraph(requirement); }
                body.blank(); body.label("Example:"); body.line(first_line(selected->examples), UWVM_AES_YELLOW, 2u);
                body.paragraph(selected->arguments);
                if(selected->key == "language frames up" || selected->key == "language frames down")
                { body.paragraph("Frame #0 is the current function; #1 is its caller. This does not run code."); }
                body.blank(); body.paragraph(::fast_io::concat_fast_io("Full details: help ", selected->key, " full").subview(0u));
            }
            if((selected->group() || requested.full) && !selected->notes.empty())
            { body.blank(); body.label("Notes:"); body.line(selected->notes, UWVM_AES_WHITE, 2u); }
            body.blank();
            if(selected->group() || requested.full)
            { if(selected->parent.empty()) { body.paragraph("Back: help"); }
              else { body.paragraph(::fast_io::concat_fast_io("Back: help ", selected->parent).subview(0u)); } }
        }
        auto const next{::fast_io::concat_fast_io("help", requested.key.empty() ? view{} : view{" "}, requested.key,
            requested.full ? view{" full"} : view{})};
        return paginate(body.text().subview(0u), selected == nullptr ? view{"UWVM debugger help"} : selected->title,
            next.subview(0u), requested.page, terminal, columns, page_rows);
    }
    [[nodiscard]] constexpr char fold(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c; }
    [[nodiscard]] constexpr bool contains_word(view text, view word) noexcept
    {
        if(word.size() > text.size()) { return false; }
        for(::std::size_t at{}; at <= text.size() - word.size(); ++at)
        {
            ::std::size_t n{}; while(n < word.size() && fold(text[at + n]) == fold(word[n])) { ++n; }
            if(n == word.size()) { return true; }
        }
        return false;
    }
    [[nodiscard]] constexpr bool matches(entry const& item, view words) noexcept
    {
        while(!words.empty())
        {
            auto const part{console_aliases::split(words)};
            if(!contains_word(item.key, part.word) && !contains_word(item.title, part.word) &&
               !contains_word(item.summary, part.word) && !contains_word(item.usage, part.word) &&
               !contains_word(item.arguments, part.word) && !contains_word(item.notes, part.word)) { return false; }
            words = part.tail;
        }
        return true;
    }
    // Bounded literal ASCII words, ANDed across static help fields. This is
    // deliberately not a regexp engine, script evaluator or symbol query.
    [[nodiscard]] inline ::fast_io::string search(view selector, bool terminal = false,
        ::std::size_t columns = 80u, ::std::size_t page_rows = 18u)
    {
        writer error{terminal, columns};
        auto invalid{[&]()
        { error.line("error: Usage: apropos WORD [WORD ...] [PAGE]", UWVM_AES_LT_RED);
          error.paragraph("Search help using literal words (case insensitive, at most 128 bytes). Example: apropos native register");
          return ::fast_io::concat_fast_io(error.text()); }};
        selector = console_aliases::trim(selector);
        if(selector.empty() || selector.size() > maximum_selector_bytes) { return invalid(); }
        auto const spelling{completion_spelling(selector)}; selector = spelling.subview(0u);
        for(unsigned char c : selector) { if(c < 32u || c > 126u) { return invalid(); } }
        ::std::size_t page{1u}; auto const space{selector.rfind(" ")};
        auto const last{space == ::fast_io::containers::npos ? selector : selector.subview(space + 1u)};
        if(!last.empty() && last.front() >= '0' && last.front() <= '9')
        {
            auto const end{last.data() + last.size()};
            auto const scanned{::fast_io::parse_by_scan(last.data(), end, ::fast_io::mnp::dec_get<true, true>(page))};
            if(scanned.code != ::fast_io::parse_code::ok || scanned.iter != end || page == 0u || space == ::fast_io::containers::npos) { return invalid(); }
            selector = console_aliases::trim(selector.subview(0u, space));
            if(selector.empty()) { return invalid(); }
        }
        writer body{terminal, columns}; ::std::size_t count{};
        for(auto const& item : entries) { if(!item.group() && matches(item, selector)) { ++count; } }
        if(count == 0u)
        { body.paragraph("No matching commands. Try a shorter word, or use help for command classes."); return ::fast_io::concat_fast_io(body.text()); }
        body.paragraph(::fast_io::concat_fast_io(::fast_io::mnp::dec(count), " matching commands. Use help COMMAND for usage and an example.").subview(0u));
        body.blank();
        for(auto const& item : entries)
        {
            if(item.group() || !matches(item, selector)) { continue; }
            auto const command{::fast_io::concat_fast_io("help ", item.key)};
            body.row(command.subview(0u), item.summary, 28u);
        }
        auto const next{::fast_io::concat_fast_io("apropos ", selector)};
        return paginate(body.text().subview(0u), "Search debugger help", next.subview(0u), page, terminal, columns, page_rows);
    }
}
#include <uwvm2/utils/ansies/ansi_pop_macro.h>
