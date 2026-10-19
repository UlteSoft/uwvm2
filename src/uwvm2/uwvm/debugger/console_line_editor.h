/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <fast_io_core.h>
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string_view.h>
# include <cstddef>
# include <memory>
# include <fast_io_dsal/string.h>
# include <fast_io.h>
# include "console_help_catalog.h"
# include "console_aliases.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_editing
{
    inline constexpr unsigned key_up{256u}, key_down{257u}, key_home{258u}, key_end{259u}, key_delete{260u}, key_word_left{261u}, key_word_right{262u}, key_word_delete{263u}, key_word_backspace{264u};
    inline constexpr ::std::size_t maximum_bytes{8448u}, maximum_history{32u}, maximum_kills{8u};
    inline constexpr unsigned maximum_argument{1024u};
    enum class action { changed, unchanged, idle, redraw, candidates, complete, end, interrupted, oversized, invalid,
        completion, tui_toggle, tui_single, tui_split, page_up, page_down, mouse };
    struct mouse_event { unsigned button{}, column{}, row{}; bool released{}; };
    // Accessed only by the console management thread (including Windows input).
    inline bool mouse_reporting{};
    struct completion_word { ::fast_io::string_view text{}; bool arguments{}; };
    // HOST command vocabulary. Argument completion is delegated to the normal
    // management thread; this editor has no filesystem or stop authority.
    inline constexpr completion_word completion_words[]{
        {"set print pretty on", false}, {"set print pretty off", false}, {"show print pretty", false},
        {"set print elements", true}, {"show print elements", false}, {"info pretty-printer", false},
        {"print /r", true}, {"print /p", true}, {"print --raw", true}, {"print --pretty", true},
        {"pretty-printer sequence", true}, {"pretty-printer string", true}, {"pretty-printer pointer", true}, {"pretty-printer optional", true},
        {"pretty-printer delete", true}, {"pretty-printer enable", true}, {"pretty-printer disable", true}, {"pretty-printer list", false},
        {"apropos", true},
        {"path create", true}, {"path extend", true}, {"path members", true}, {"path clear", false},
        {"advance", true}, {"until", true}, {"condition", true}, {"display", true}, {"info display", false}, {"undisplay", true},
        {"enable display", true}, {"disable display", true}, {"delete display", true},
        {"selection range", true}, {"selection copy", false}, {"selection show", false}, {"selection clear", false},
        {"io clear", false}, {"io show", false}, {"io save", true}, {"io input", true}, {"io follow on", false}, {"io follow off", false},
        {"layout add", true}, {"layout remove", true}, {"layout config save", true}, {"layout config load", true}, {"layout move", true}, {"layout tree", true},
        {"jump src", true}, {"jump wasm", true}, {"jump asm", true},
        {"inspect src", true}, {"inspect wasm local", true}, {"inspect wasm stack", true}, {"inspect asm", true},
        {"memory select", true},
        {"memory format all", false},
        {"memory format char", false},
        {"memory format wc", false},
        {"memory format utf8", false},
        {"memory format utf16", false},
        {"memory format utf32", false},
        {"memory format i8", false},
        {"memory format u8", false},
        {"memory format i16", false},
        {"memory format u16", false},
        {"memory format i32", false},
        {"memory format u32", false},
        {"memory format i64", false},
        {"memory format u64", false},
        {"memory format f32", false},
        {"memory format f64", false},
        {"window show", false}, {"window reset", false}, {"window resize", true}, {"scroll", true},
        {"mouse on", false}, {"mouse off", false}, {"info wasip1 fd all", false},
        {"info wasip1 fd", true}, {"list src", false}, {"list wasm", false}, {"list asm", false},
        {"tui enable", false}, {"tui disable", false}, {"set source", true},
        {"layout src", false}, {"layout asm", false},
        {"layout regs", false}, {"layout wasm", false}, {"layout wasip1", false},
        {"layout locals", false}, {"layout stack", false}, {"layout operands", false},
        {"layout memory", false}, {"layout io", false}, {"layout horizontal", true}, {"layout vertical", true},
        {"layout next", false}, {"focus next", false}, {"focus prev", false},
        {"focus src", false}, {"focus wasm", false}, {"focus asm", false}, {"focus regs", false},
        {"focus locals", false}, {"focus stack", false}, {"focus memory", false}, {"focus wasip1", false}, {"focus io", false},
        {"focus cmd", false}, {"focus code", false},
        {"backtrace", false},
        {"breakpoint delete", true},
        {"breakpoint list", false},
        {"breakpoint enable", true}, {"breakpoint disable", true},
        {"frame select", true},
        {"frame variable", true},
        {"process continue", false},
        {"process interrupt", false},
        {"process status", false},
        {"register read", true},
        {"thread backtrace", false},
        {"thread list", false},
        {"thread step-in", false},
        {"thread step-inst", false},
        {"thread step-inst-over", false},
        {"thread step-out", false},
        {"thread step-over", false},
        {"breakwasm", true},
        {"breakasm", true}, {"bi", true}, {"bw", true},
        {"breaksource", true},
        {"bs", true},
        {"info modules", true},
        {"bt", false},
        {"catch wasm", true},
        {"continue", false},
        {"delete", true},
        {"disable", true},
        {"disas", false},
        {"disassemble", true},
        {"disassemble-range", true},
        {"down", false},
        {"enable", true},
        {"finish", false},
        {"frame", false},
        {"frames", false},
        {"globals", true},
        {"help", true},
        {"info breakpoints", false},
        {"info locals", false}, {"info all-registers", false}, {"info saved", true},
        {"info controls", true}, {"info handlers", true}, {"info control-params", true},
        {"info control-results", true}, {"info handler-params", true},
        {"info globals", true},
        {"info operands", true},
        {"info registers", false},
        {"info table", true},
        {"finish asm", false},
        {"fin asm", false},
        {"info threads", false},
        {"info wasm-events", false},
        {"info wasip1 args", true},
        {"info wasip1 env", true},
        {"info wasip1 fds", true},
        {"info wasip1 preopens", true},
        {"locals", true},
        {"locals source", true},
        {"locals wasm", true},
        {"members", true},
        {"memory", true},
        {"next", false},
        {"nexti", false},
        {"ni", false},
        {"operands", true},
        {"pause", false},
        {"print", true},
        {"print-frame", true},
        {"ptype", true},
        {"ptype-frame", true},
        {"quit", false},
        {"replace", true},
        {"run", false},
        {"start", false}, {"st", false}, {"startw", false}, {"stw", false},
        {"starti", false}, {"sti", false}, {"kill", false},
        {"saved", true}, {"controls", true}, {"handlers", true}, {"control-params", true},
        {"control-results", true}, {"handler-params", true}, {"ignore", true}, {"frames wasm", true},
        {"set wasm memory", true}, {"set wasm global", true}, {"set wasm table", true}, {"set wasm member", true},
        {"set wasip1 arg", true},
        {"set wasip1 arg-insert", true}, {"set wasip1 file", true}, {"set wasip1 fd-dup", true},
        {"set wasip1 checkpoint", true}, {"set wasip1 restore", true},
        {"set wasip1 export", true}, {"set wasip1 import", true},
        {"set wasip1 export-group", true}, {"set wasip1 import-group", true},
        {"set wasip1 env", true},
        {"set wasip1 rights", true},
        {"si", false},
        {"status", false},
        {"step", false},
        {"step asm", true},
        {"step source", true},
        {"step wasm", true}, {"next wasm", true}, {"finish wasm", true},
        {"step src", true}, {"next src", true}, {"finish src", true}, {"next asm", true},
        {"sw", true}, {"nw", true}, {"fw", true}, {"fi", true}, {"ss", true}, {"ns", true}, {"fs", true},
        {"input", true},
        {"table", true},
        {"trace wasm", true},
        {"trace wasm on", true}, {"trace wasm off", false}, {"trace wasm clear", false}, {"trace wasm read", true},
        {"trace wasip1 on", true}, {"trace wasip1 off", false}, {"trace wasip1 clear", false}, {"trace wasip1 read", true},
        {"unset wasip1 arg", true}, {"unset wasip1 fd", true}, {"unset wasip1 checkpoint", true},
        {"unset wasip1 env", true},
        {"up", false},
        {"wait", false},
        {"wasm-script", true},
        {"where", false},
    };
    [[nodiscard]] inline bool safe_completion_text(::fast_io::string_view text) noexcept
    {
        for(::std::size_t i{}; i<text.size();)
        {
            auto const c{static_cast<unsigned char>(text[i])};
            if(c<32u || c==127u) { return false; }
            if(c<128u) { ++i; continue; }
            auto const n{c>=194u && c<=223u ? 2u : c>=224u && c<=239u ? 3u : c>=240u && c<=244u ? 4u : 0u};
            if(n==0u || n>text.size()-i) { return false; }
            auto const next{static_cast<unsigned char>(text[i+1u])};
            if((c==194u && next<=159u)||(c==224u && next<160u)||(c==237u && next>=160u)||
               (c==240u && next<144u)||(c==244u && next>=144u)) { return false; }
            for(unsigned j{1u};j<n;++j) { if((static_cast<unsigned char>(text[i+j])&0xc0u)!=0x80u) { return false; } }
            i+=n;
        }
        return true;
    }
    inline constexpr ::std::size_t maximum_static_candidates{sizeof(completion_words) / sizeof(completion_word)};
    struct command_choices
    {
        ::fast_io::array<completion_word, maximum_static_candidates> words{};
        ::std::size_t size{}, begin{}, end{};
    };
    [[nodiscard]] inline constexpr bool word_space(char c) noexcept { return ::fast_io::char_category::c_space::char_is(c); }
    struct help_choices : command_choices { bool recognized{}; };
    // Match a normalized documentation prefix, but edit only the current raw
    // token. Earlier separators, trailing selectors and the caret stay owned
    // by the line editor; candidates remain views into the static catalogue.
    [[nodiscard]] inline help_choices help_matches(::fast_io::string_view line, ::std::size_t cursor)
    {
        help_choices result{};
        if(cursor > line.size() || line.size() > maximum_bytes) { return result; }
        ::std::size_t head{}; while(head < line.size() && word_space(line[head])) { ++head; }
        auto end{head}; while(end < line.size() && !word_space(line[end])) { ++end; }
        auto const root{line.subview(head, end - head)};
        if((root != "help" && root != "h") || end == line.size() || cursor <= end) { return result; }
        result.recognized = true; result.begin = result.end = cursor;
        while(result.begin > end && !word_space(line[result.begin - 1u])) { --result.begin; }
        while(result.end < line.size() && !word_space(line[result.end])) { ++result.end; }
        if(cursor == result.begin && cursor != result.end) { return result; }
        auto const parent{console_help::completion_spelling(line.subview(end, result.begin - end), true)};
        auto const typed{line.subview(result.begin, cursor - result.begin)};
        auto const selector{::fast_io::concat_fast_io(parent, typed)};
        if(selector.size() > console_help::maximum_selector_bytes) { return result; }
        if(cursor == result.end && !typed.empty())
        {
            auto const exact{console_help::find(console_help::canonical(selector.subview(0u)))};
            if(exact != nullptr && exact->group())
            {
                // Root category aliases (source -> language) keep their old
                // navigation behavior without rewriting unrelated separators.
                auto word{exact->key};
                if(!parent.empty())
                {
                    if(exact->key.starts_with(parent.subview(0u))) { word = exact->key.subview(parent.size()); }
                    else { for(auto const& alias : console_help::aliases)
                        { if(alias.name == selector.subview(0u)) { word = alias.name.subview(parent.size()); break; } } }
                }
                result.words[result.size++] = {word, true}; return result;
            }
        }
        console_help::visit_completions(selector.subview(0u), [&](::fast_io::string_view name)
        {
            auto const word{name.subview(parent.size())};
            for(::std::size_t i{}; i < result.size; ++i) { if(result.words[i].text == word) { return; } }
            if(result.size == result.words.size()) { return; }
            auto const page{console_help::find(console_help::canonical(name))};
            result.words[result.size++] = {word, page != nullptr && page->group()};
        });
        return result;
    }
    // Resolve only the completed command prefix. Candidate views point into
    // static vocabulary; argument bytes are never parsed as a completion name.
    [[nodiscard]] inline command_choices command_matches(::fast_io::string_view line, ::std::size_t cursor)
    {
        command_choices result{};
        if(cursor > line.size() || line.size() > maximum_bytes) { return result; }
        result.begin = result.end = cursor;
        while(result.begin != 0u && !word_space(line[result.begin - 1u])) { --result.begin; }
        while(result.end < line.size() && !word_space(line[result.end])) { ++result.end; }
        if(cursor == result.begin && result.end != result.begin) { return result; }
        auto const prefix{line.subview(result.begin, cursor - result.begin)};
        auto const previous{console_aliases::trim(line.subview(0u, result.begin))};
        ::fast_io::string parent{};
        if(!previous.empty())
        {
            auto const normalized{console_aliases::normalize(previous)};
            if(normalized.ambiguous) { return result; }
            auto const parts{console_aliases::split(previous)};
            auto root{normalized.head};
            if(root.empty())
            {
                // TUI/editing commands also have a static vocabulary, although
                // they need not belong to the runtime command parser.
                for(auto const& word : completion_words)
                { if(console_aliases::split(word.text).word == parts.word) { root = parts.word; break; } }
                if(root.empty()) { return result; }
            }
            if(root == "info" && normalized.valid) { parent = ::fast_io::concat_fast_io(normalized.view()); }
            else
            {
                parent = ::fast_io::concat_fast_io(root);
                auto tail{parts.tail};
                while(!tail.empty())
                { auto const next{console_aliases::split(tail)}; parent.append(" "); parent.append(next.word); tail = next.tail; }
            }
            parent.append(" ");
        }
        for(auto const& word : completion_words)
        {
            if(!word.text.starts_with(parent.subview(0u))) { continue; }
            auto const tail{word.text.subview(parent.size())};
            auto const boundary{tail.find(" ")};
            auto const child{tail.subview(0u, boundary == ::fast_io::containers::npos ? tail.size() : boundary)};
            if(child.empty() || !child.starts_with(prefix)) { continue; }
            bool const arguments{boundary != ::fast_io::containers::npos || word.arguments};
            ::std::size_t i{};
            while(i != result.size && result.words[i].text != child) { ++i; }
            if(i == result.size) { result.words[result.size++] = {child, arguments}; }
            else { result.words[i].arguments = result.words[i].arguments || arguments; }
        }
        return result;
    }
    struct buffer
    {
        ::fast_io::array<char, maximum_bytes> bytes{};
        ::std::size_t size{};
        [[nodiscard]] ::fast_io::string_view view() const noexcept { return ::fast_io::string_view{bytes.data(), size}; }
    };
    // Host-input editing only. Fixed owned edit buffers; completion may allocate
    // host text. No filesystem, evaluation, runtime identity or guest memory.
    class editor final
    {
        buffer line_{}, draft_{}, repeat_{}, search_query_{}, search_draft_{};
        ::fast_io::array<buffer, maximum_history> history_{};
        ::fast_io::array<buffer, maximum_kills> kills_{};
        ::std::size_t cursor_{}, count_{}, selected_{};
        ::std::size_t kill_count_{}, yank_selected_{}, yank_begin_{}, yank_size_{};
        unsigned escape_{};
        ::fast_io::array<char, 32u> csi_{};
        mouse_event mouse_{};
        ::std::size_t csi_size_{}, paste_end_size_{};
        bool pasting_{}, quoted_{}, next_history_pending_{};
        buffer next_history_{};
        unsigned argument_{};
        bool argument_pending_{}, argument_negative_{};
        bool overflow_{}, swallow_lf_{}, searching_{}, search_matched_{}, search_overflow_{};
        bool last_kill_{}, last_yank_{};
        ::std::size_t search_before_{}, search_cursor_{};
        struct undo_record { buffer text{}; ::std::size_t cursor{}; };
        static constexpr ::std::size_t maximum_undo{32u};
        ::fast_io::array<undo_record, maximum_undo> undo_{};
        ::std::size_t undo_count_{};
        bool insertion_group_{};
        buffer remembered_query_{};
        bool search_forward_{};
        ::std::size_t search_origin_{};
        void save_undo(bool insertion = false) noexcept
        {
            if(insertion && insertion_group_) { return; }
            if(undo_count_ == maximum_undo)
            {
                // [owned undo0..31] end
                // [safe] checked full extent; i in1..31, destination i-1 in0..30.
                for(::std::size_t i{1u}; i != maximum_undo; ++i) { undo_[i - 1u] = undo_[i]; }
                --undo_count_;
            }
            // [owned undo0..31][count <32] end
            // [safe] line/cursor are owned <=maximum_bytes; no borrowed text.
            undo_[undo_count_++] = {line_, cursor_}; insertion_group_ = insertion;
        }
        [[nodiscard]] action undo() noexcept
        {
            insertion_group_ = last_kill_ = last_yank_ = false;
            if(undo_count_ == 0u) { return action::unchanged; }
            auto const& entry{undo_[undo_count_ - 1u]};
            if(entry.text.size > maximum_bytes || entry.cursor > entry.text.size) { return action::invalid; }
            line_ = entry.text; cursor_ = entry.cursor; --undo_count_;
            return action::changed;
        }
        [[nodiscard]] bool word_byte(::std::size_t at) const noexcept
        {
            // [owned line0..size<=maximum_bytes] end
            // [safe] bound precedes indexing; FastIO performs C letters/digits
            // classification, independent of global locale or signed-char UB.
            return at < line_.size && ::fast_io::char_category::is_c_alnum(line_.bytes[at]);
        }
        [[nodiscard]] ::std::size_t word_before(::std::size_t at) const noexcept
        {
            if(at > line_.size) { return line_.size; }
            // Every previous() checks at>0 and moves over whole UTF8 bytes.
            // [owned 0 ... at<=size] end; no negative index can be formed.
            while(at != 0u && !word_byte(previous(at))) { at = previous(at); }
            while(at != 0u && word_byte(previous(at))) { at = previous(at); }
            return at;
        }
        [[nodiscard]] ::std::size_t word_after(::std::size_t at) const noexcept
        {
            if(at > line_.size) { return line_.size; }
            // [owned at ... size<=maximum_bytes] end
            // [safe] at<size precedes both classification and whole UTF8 next.
            while(at < line_.size && !word_byte(at)) { at = next(at); }
            while(at < line_.size && word_byte(at)) { at = next(at); }
            return at;
        }
        void remember_search() noexcept
        {
            if(!search_overflow_ && search_query_.size != 0u) { remembered_query_ = search_query_; }
        }
        [[nodiscard]] action initial_search() noexcept
        {
            // search_origin<=count<=32; +1 only for an actual history index.
            auto const begin{search_forward_ ? search_origin_ : search_origin_ < count_ ? search_origin_ + 1u : count_};
            return search_from(begin, search_forward_);
        }

        static void copy(buffer& out, ::fast_io::string_view text) noexcept
        {
            out.size = text.size();
            for(::std::size_t i{}; i != text.size(); ++i) { out.bytes[i] = text[i]; }
        }
        [[nodiscard]] bool continuation(::std::size_t index) const noexcept
        { return index < line_.size && (static_cast<unsigned char>(line_.bytes[index]) & 0xc0u) == 0x80u; }
        [[nodiscard]] ::std::size_t previous(::std::size_t at) const noexcept
        { if(at == 0u) { return 0u; } --at; while(at != 0u && continuation(at)) { --at; } return at; }
        [[nodiscard]] ::std::size_t next(::std::size_t at) const noexcept
        { if(at >= line_.size) { return line_.size; } ++at; while(at < line_.size && continuation(at)) { ++at; } return at; }
        void erase(::std::size_t begin, ::std::size_t end) noexcept
        {
            if(begin > end || end > line_.size) { return; }
            for(::std::size_t i{end}; i != line_.size; ++i) { line_.bytes[begin + i - end] = line_.bytes[i]; }
            line_.size -= end - begin; cursor_ = begin;
        }
        [[nodiscard]] action kill(::std::size_t begin, ::std::size_t end, bool backward) noexcept
        {
            if(begin > end || end > line_.size || begin == end) { return action::unchanged; }
            auto const size{end - begin}; save_undo();
            // [owned line begin ... end <= size <= maximum_bytes][owned kill[0..7]]
            // [safe] size fits one whole item; adjacent kills coalesce only
            // after their combined extent is checked. No killed suffix is lost.
            if(last_kill_ && kill_count_ != 0u && size <= maximum_bytes - kills_[0].size)
            {
                auto& item{kills_[0]};
                if(backward)
                {
                    // [safe] item.size+size <= capacity; reverse shift never
                    // reads a moved source or writes at/after the array end.
                    for(auto i{item.size}; i != 0u; --i) { item.bytes[i + size - 1u] = item.bytes[i - 1u]; }
                    for(::std::size_t i{}; i != size; ++i) { item.bytes[i] = line_.bytes[begin + i]; }
                }
                else
                { for(::std::size_t i{}; i != size; ++i) { item.bytes[item.size + i] = line_.bytes[begin + i]; } }
                item.size += size;
            }
            else
            {
                if(kill_count_ != maximum_kills) { ++kill_count_; }
                for(auto i{kill_count_ - 1u}; i != 0u; --i) { kills_[i] = kills_[i - 1u]; }
                auto& item{kills_[0]}; item.size = size;
                for(::std::size_t i{}; i != size; ++i) { item.bytes[i] = line_.bytes[begin + i]; }
            }
            erase(begin, end); last_kill_ = true; last_yank_ = false;
            return action::changed;
        }
        void insert_kill(::std::size_t selected) noexcept
        {
            // Called only with selected < kill_count <= 8 and capacity already
            // proved by yank/yank_pop. Source and destination arrays are disjoint.
            auto const& item{kills_[selected]}; auto const begin{cursor_};
            // [line prefix ... cursor ... size][free >= item.size] end
            // [safe] backward shift reads < old size and writes < new size.
            for(auto i{line_.size}; i != begin; --i) { line_.bytes[i + item.size - 1u] = line_.bytes[i - 1u]; }
            for(::std::size_t i{}; i != item.size; ++i) { line_.bytes[begin + i] = item.bytes[i]; }
            line_.size += item.size; cursor_ += item.size;
            yank_begin_ = begin; yank_size_ = item.size; yank_selected_ = selected;
            last_yank_ = true; last_kill_ = false;
        }
        [[nodiscard]] action yank() noexcept
        {
            if(kill_count_ == 0u || kills_[0].size > maximum_bytes - line_.size)
            { last_yank_ = false; return action::unchanged; }
            save_undo(); insert_kill(0u); return action::changed;
        }
        [[nodiscard]] action yank_pop() noexcept
        {
            if(!last_yank_ || kill_count_ < 2u || yank_selected_ >= kill_count_ || yank_begin_ > line_.size ||
               yank_size_ > line_.size - yank_begin_ || cursor_ != yank_begin_ + yank_size_)
            { return action::unchanged; }
            auto const selected{yank_selected_ + 1u == kill_count_ ? 0u : yank_selected_ + 1u};
            // Check the replacement's whole extent BEFORE erasing the actual
            // previously yanked range. Overflow leaves text and cursor unchanged.
            if(kills_[selected].size > maximum_bytes - (line_.size - yank_size_)) { return action::unchanged; }
            save_undo(); erase(yank_begin_, yank_begin_ + yank_size_);
            insert_kill(selected); return action::changed;
        }
        [[nodiscard]] action recall(bool older) noexcept
        {
            if(count_ == 0u || (older ? selected_ == 0u : selected_ == count_)) { return action::unchanged; }
            if(selected_ == count_) { draft_ = line_; }
            if(older) { --selected_; } else { ++selected_; }
            line_ = selected_ == count_ ? draft_ : history_[selected_]; cursor_ = line_.size;
            undo_count_ = 0u; insertion_group_ = false; // a different real history line has its own edit history.
            return action::changed;
        }
        [[nodiscard]] action search_from(::std::size_t before, bool forward = false) noexcept
        {
            if(before > count_) { return action::invalid; }
            bool const retain{search_matched_ && line_.view().find(search_query_.view()) != ::fast_io::containers::npos};
            auto accept{[&](::std::size_t index) noexcept
            {
                // [owned history0..count<=32] end
                // [safe] both scan branches prove index<count BEFORE this copy.
                auto const value{history_[index].view()};
                if(value.find(search_query_.view()) == ::fast_io::containers::npos) { return false; }
                line_ = history_[index]; cursor_ = line_.size; search_before_ = search_origin_ = index;
                // A later query edit stays anchored to this actual current
                // match, including after direction changes/repeated searches.
                search_matched_ = true; return true;
            }};
            if(forward)
            {
                for(auto index{before}; index < count_; ++index) { if(accept(index)) { return action::changed; } }
            }
            else
            {
                for(auto index{before}; index != 0u;)
                { --index; if(accept(index)) { return action::changed; } }
            }
            search_matched_ = retain; return action::changed;
        }
        [[nodiscard]] action complete_word() noexcept
        {
            auto const prefix{line_.view()};
            auto const help{help_matches(prefix, cursor_)};
            if(help.recognized)
            {
                if(help.size == 0u) { return action::unchanged; }
                auto const& first{help.words[0u]}; auto common{first.text.size()};
                for(::std::size_t n{1u}; n < help.size; ++n)
                { ::std::size_t i{}; while(i < common && i < help.words[n].text.size() && first.text[i] == help.words[n].text[i]) { ++i; } common = i; }
                if(help.size == 1u || common > cursor_ - help.begin)
                {
                    auto const completed{::fast_io::concat_fast_io(first.text.subview(0u, common),
                        help.size == 1u && first.arguments && help.end == line_.size ? ::fast_io::string_view{" "} : ::fast_io::string_view{})};
                    auto const changed{replace_completion(help.begin, help.end, completed.subview(0u))};
                    if(changed == action::changed) { return changed; }
                }
                return help.size > 1u ? action::candidates : action::unchanged;
            }
            auto const matches{command_matches(prefix, cursor_)};
            if(matches.size == 0u) { return action::unchanged; }
            auto const typed{prefix.subview(matches.begin, cursor_ - matches.begin)};
            for(::std::size_t i{}; i != matches.size; ++i)
            {
                if(matches.words[i].text != typed) { continue; }
                auto const replacement{::fast_io::concat_fast_io(typed,
                    matches.words[i].arguments && matches.end == line_.size ? ::fast_io::string_view{" "} : ::fast_io::string_view{})};
                auto const changed{replace_completion(matches.begin, matches.end, replacement.subview(0u))};
                // A sole complete literal still requests the existing redraw;
                // it adds neither an undo entry nor a command submission.
                return changed == action::unchanged && matches.size == 1u ? action::changed : changed;
            }
            auto const& first{matches.words[0u]}; auto common{first.text.size()};
            for(::std::size_t i{1u}; i != matches.size; ++i)
            { ::std::size_t n{}; while(n != common && n != matches.words[i].text.size() && first.text[n] == matches.words[i].text[n]) { ++n; } common = n; }
            if(matches.size == 1u || common > cursor_ - matches.begin)
            {
                auto const replacement{::fast_io::concat_fast_io(first.text.subview(0u, common),
                    matches.size == 1u && first.arguments && matches.end == line_.size ? ::fast_io::string_view{" "} : ::fast_io::string_view{})};
                auto const changed{replace_completion(matches.begin, matches.end, replacement.subview(0u))};
                if(changed == action::changed) { return changed; }
            }
            return matches.size > 1u ? action::candidates : action::unchanged;
        }
    public:
        [[nodiscard]] action transpose() noexcept
        {
            if(cursor_ == 0u || line_.size < 2u) { return action::unchanged; }
            auto const middle{cursor_ == line_.size ? previous(cursor_) : cursor_};
            if(middle == 0u) { return action::unchanged; }
            auto const first{previous(middle)}, end{next(middle)};
            save_undo(); auto const original{line_};
            ::std::size_t out{first};
            for(auto i{middle}; i < end; ++i) { line_.bytes[out++] = original.bytes[i]; }
            for(auto i{first}; i < middle; ++i) { line_.bytes[out++] = original.bytes[i]; }
            cursor_ = end; return action::changed;
        }
        [[nodiscard]] action insert(unsigned key, bool group = true) noexcept
        {
            if(line_.size == maximum_bytes) { overflow_ = true; return action::unchanged; }
            save_undo(group);
            for(auto i{line_.size}; i != cursor_; --i) { line_.bytes[i] = line_.bytes[i - 1u]; }
            line_.bytes[cursor_++] = static_cast<char>(key); ++line_.size;
            return action::changed;
        }
        [[nodiscard]] action paste_byte(unsigned key) noexcept
        {
            // Newlines/tabs in paste are text separators, never delimiters or
            // editing keys. Exactly one bounded ending marker retires paste.
            constexpr ::fast_io::string_view ending{"\x1b[201~"};
            if(paste_end_size_ != 0u || key == 27u)
            {
                if(key == static_cast<unsigned char>(ending[paste_end_size_]))
                {
                    if(++paste_end_size_ == ending.size())
                    { pasting_ = false; paste_end_size_ = 0u; insertion_group_ = false; return action::changed; }
                }
                else { overflow_ = true; paste_end_size_ = key == 27u ? 1u : 0u; }
                return action::unchanged;
            }
            if(overflow_) { return action::unchanged; }
            if(key == '\n' || key == '\r' || key == '\t') { key = ' '; }
            if(key < 32u || key >= 256u || key == 127u) { overflow_ = true; return action::unchanged; }
            return insert(key);
        }
        void begin() noexcept
        { line_ = {}; draft_ = {}; cursor_ = 0u; selected_ = count_; escape_ = 0u; overflow_ = false;
          searching_ = search_matched_ = search_overflow_ = false; search_query_ = {}; search_draft_ = {}; search_before_ = search_cursor_ = 0u;
          last_kill_ = last_yank_ = insertion_group_ = false; undo_count_ = 0u; search_forward_ = false; search_origin_ = count_;
          csi_size_ = paste_end_size_ = argument_ = 0u; pasting_ = quoted_ = argument_pending_ = argument_negative_ = false;
          if(next_history_pending_) { line_ = next_history_; cursor_ = line_.size; next_history_pending_ = false; } }
        [[nodiscard]] bool searching() const noexcept { return searching_; }
        [[nodiscard]] bool search_matched() const noexcept { return search_matched_; }
        [[nodiscard]] bool searching_forward() const noexcept { return searching_ && search_forward_; }
        [[nodiscard]] ::fast_io::string_view search_query() const noexcept { return search_query_.view(); }
        template<typename Display> void display_candidates(Display display) const
        {
            auto const text{line_.view()};
            auto const help{help_matches(text, cursor_)};
            if(help.recognized)
            {
                auto const prefix{console_help::completion_spelling(text.subview(0u, help.begin), true)};
                for(::std::size_t i{}; i < help.size; ++i)
                { auto const candidate{::fast_io::concat_fast_io(prefix, help.words[i].text)}; display(candidate.subview(0u)); }
                return;
            }
            auto const matches{command_matches(text, cursor_)};
            for(::std::size_t i{}; i != matches.size; ++i)
            { auto const name{::fast_io::concat_fast_io(text.subview(0u, matches.begin), matches.words[i].text)}; display(name.subview(0u)); }
        }
        [[nodiscard]] buffer const& line() const noexcept { return line_; }
        [[nodiscard]] mouse_event mouse() const noexcept { return mouse_; }
        [[nodiscard]] ::std::size_t cursor() const noexcept { return cursor_; }
        // Atomic, single-undo replacement of a checked host-input span. A
        // provider can insert printable DATA only, never submit a command.
        [[nodiscard]] action replace_completion(::std::size_t begin, ::std::size_t end,
            ::fast_io::string_view text) noexcept
        {
            if(begin > cursor_ || cursor_ > end || end > line_.size ||
               text.size() > maximum_bytes - (line_.size - (end - begin))) { return action::unchanged; }
            if(!safe_completion_text(text)) { return action::unchanged; }
            if(text == line_.view().subview(begin, end - begin)) { return action::unchanged; }
            save_undo(); auto const old{line_}; ::std::size_t out{begin};
            for(char c : text) { line_.bytes[out++] = c; }
            cursor_ = out;
            for(auto i{end}; i < old.size; ++i) { line_.bytes[out++] = old.bytes[i]; }
            line_.size = out; return action::changed;
        }
        [[nodiscard]] ::std::size_t history_size() const noexcept { return count_; }
        [[nodiscard]] ::std::size_t kill_ring_size() const noexcept { return kill_count_; }
        // Caller decides repetition eligibility after its real command parser:
        // restore/replace/scripts/invalid commands must never become empty-Enter
        // replay candidates. Metadata/history here grants no control permission.
        void remember(::fast_io::string_view text, bool repeatable) noexcept
        {
            if(text.empty() || text.size() > maximum_bytes) { return; }
            if(count_ == maximum_history)
            { for(::std::size_t i{1u}; i != count_; ++i) { history_[i - 1u] = history_[i]; } --count_; }
            copy(history_[count_++], text); selected_ = count_;
            if(repeatable) { copy(repeat_, text); } else { repeat_ = {}; }
        }
        void set_repeat(::fast_io::string_view text,bool eligible) noexcept
        { if(eligible) { copy(repeat_,text); }else { repeat_={}; } }
        void forget_repeat() noexcept { repeat_ = {}; }
        [[nodiscard]] action feed(int byte) noexcept
        {
            if(byte==-5) { return action::idle; }
            if(swallow_lf_) { swallow_lf_ = false; if(byte == '\n') { return action::unchanged; } }
            if(byte == -3 || byte == 3) { next_history_pending_ = false; begin(); repeat_ = {}; return action::interrupted; }
            if(byte == -1)
            {
                if(overflow_ || escape_ != 0u || pasting_ || quoted_ || argument_pending_ || (searching_ && !search_matched_)) { return action::oversized; }
                // EOF accepts only the same actual matching line as Enter;
                // remember its bounded query before retiring the input state.
                if(searching_) { remember_search(); searching_ = false; }
                return line_.size == 0u ? action::end : action::complete;
            }
            if(byte < 0 || byte > static_cast<int>(key_word_backspace)) { return action::invalid; }
            if(pasting_) { return paste_byte(static_cast<unsigned>(byte)); }
            if(quoted_)
            {
                quoted_ = false;
                if(byte == '\n' || byte == '\r' || byte == '\t') { byte = ' '; }
                if(byte < 32 || byte >= 256 || byte == 127) { overflow_ = true; return action::unchanged; }
                return insert(static_cast<unsigned>(byte), false);
            }
            // An incomplete escape must consume no editor control or newline.
            if(escape_ != 0u && (byte < 32 || byte == 127) && !(escape_ == 6u && (byte == 21 || byte == 1)) &&
                !(escape_ == 1u && byte == 127) && escape_ != 7u)
            { if(byte == '\n' || byte == '\r') { return action::oversized; } overflow_ = true; escape_ = 0u; return action::unchanged; }
            bool const meta_kill{escape_ == 1u && (byte == 'd' || byte == 127)};
            if(byte != 11 && byte != 21 && byte != 23 && byte != 27 && !meta_kill) { last_kill_ = false; }
            if(byte < 32 || byte == 127 || byte >= static_cast<int>(key_up) || escape_ != 0u) { insertion_group_ = false; }
            // Escape is a prefix, not an intervening editing operation. Only
            // its immediate y continuation can retain yank-pop eligibility.
            if(byte != 25 && byte != 27 && !(escape_ == 1u && byte == 'y')) { last_yank_ = false; }
            if(searching_)
            {
                if(byte == 7) // Ctrl+G restores the actual pre-search draft/cursor.
                {
                    remember_search(); line_ = search_draft_; cursor_ = search_cursor_;
                    searching_ = false; search_overflow_ = false; return action::changed;
                }
                if(search_overflow_) { return action::unchanged; } // discard full oversized query until host cancel.
                if(byte == 18 || byte == 19)
                {
                    search_forward_ = byte == 19;
                    if(search_query_.size == 0u && remembered_query_.size != 0u)
                    {
                        search_query_ = remembered_query_; search_origin_ = search_before_; return initial_search();
                    }
                    // current match is a genuine index<count, or initial count.
                    // +1 is bounded by count; direction changes never wrap.
                    auto const from{search_forward_ && search_before_ < count_ ? search_before_ + 1u : search_before_};
                    return search_from(from, search_forward_);
                }
                if(byte == 8 || byte == 127)
                {
                    if(search_query_.size != 0u)
                    {
                        --search_query_.size;
                        while(search_query_.size != 0u &&
                            (static_cast<unsigned char>(search_query_.bytes[search_query_.size]) & 0xc0u) == 0x80u) { --search_query_.size; }
                    }
                    return initial_search();
                }
                if(byte >= 32 && byte < 256)
                {
                    if(search_query_.size == maximum_bytes)
                    { search_overflow_ = true; search_matched_ = false; return action::changed; }
                    // [owned query0..capacity] end
                    // [safe] size<maximum_bytes precedes write and increment.
                    search_query_.bytes[search_query_.size++] = static_cast<char>(byte); return initial_search();
                }
                if((byte == '\n' || byte == '\r') && !search_matched_) { return action::unchanged; }
                remember_search();
                if(!search_matched_) { line_ = search_draft_; cursor_ = search_cursor_; }
                else
                {
                    // Accepting a different real history line retires only its
                    // host edit history; no guest mutation or resume occurs.
                    undo_count_ = 0u; insertion_group_ = false;
                    selected_ = search_before_; draft_ = search_draft_;
                }
                searching_ = false;
            }
            if(byte == 18 || byte == 19)
            {
                search_draft_ = line_; search_cursor_ = cursor_; search_query_ = {};
                searching_ = true; search_matched_ = search_overflow_ = false; search_forward_ = byte == 19;
                search_origin_ = search_before_ = selected_; return initial_search();
            }
            if(byte == 12) { return action::redraw; } // Ctrl+L redraws only the HOST display
            if(byte == 9)
            {
                auto const result{complete_word()};
                if(result != action::unchanged) { return result; }
                auto const text{line_.view()}; auto const parts{console_aliases::split(text)};
                // Decide from command words, retaining original argument bytes
                // and caret offsets for the path/symbol provider.
                if(!parts.tail.empty() || (!text.empty() && word_space(text.back())))
                {
                    auto const normalized{console_aliases::normalize(text)};
                    constexpr ::fast_io::string_view roots[]{"until", "advance", "display", "condition", "replace", "set", "breaksource", "bs",
                        "breakwasm", "bw", "breakasm", "bi", "window", "scroll", "io", "inspect", "layout", "focus", "info", "print", "p", "print-frame", "ptype", "ptype-frame", "source-value", "source-type", "frame"};
                    auto const command{normalized.head.empty() ? parts.word : normalized.head};
                    if(command=="set" && console_aliases::split(parts.tail).word!="source") { return action::unchanged; }
                    if(!normalized.ambiguous)
                    { for(auto root : roots) { if(command == root) { return action::completion; } } }
                }
                return action::unchanged;
            }
            if(byte == '\n' || byte == '\r' || byte == 15)
            {
                if(argument_pending_) { argument_pending_ = false; return action::unchanged; }
                if(byte == 15 && selected_ < count_ && selected_ + 1u < count_)
                { next_history_ = history_[selected_ + 1u]; next_history_pending_ = true; }
                swallow_lf_ = byte == '\r';
                if(overflow_ || escape_ != 0u) { return action::oversized; }
                if(line_.size == 0u && repeat_.size != 0u) { line_ = repeat_; cursor_ = line_.size; }
                return action::complete;
            }
            if(overflow_) { return action::unchanged; } // discard complete oversized command through delimiter
            unsigned key{static_cast<unsigned>(byte)};
            if(escape_ != 0u)
            {
                if(escape_ == 6u)
                {
                    escape_ = 0u;
                    if(key == 21u) { return undo(); } // Ctrl+X Ctrl+U.
                    if(key == 'a' || key == 'A' || key == 1u) { return action::tui_toggle; }
                    if(key == '1') { return action::tui_single; }
                    if(key == '2') { return action::tui_split; }
                    overflow_ = true; return action::unchanged; // reject the WHOLE unsupported sequence.
                }
                if((escape_ == 1u && (key >= '0' && key <= '9')) || escape_ == 7u)
                {
                    if(escape_ == 1u) { argument_pending_ = true; argument_ = 0u; argument_negative_ = false; }
                    if(key >= '0' && key <= '9')
                    {
                        if(argument_ > maximum_argument / 10u || argument_ * 10u + key - '0' > maximum_argument)
                        { overflow_ = true; escape_ = 0u; return action::unchanged; }
                        argument_ = argument_ * 10u + key - '0'; escape_ = 7u; return action::unchanged;
                    }
                    if(key == 27u) { escape_ = 1u; return action::unchanged; }
                    escape_ = 0u; // execute the first non-digit through normal decoding below
                }
                else if(escape_ == 1u && key == '-')
                { argument_pending_ = argument_negative_ = true; argument_ = 0u; escape_ = 7u; return action::unchanged; }
                else if(escape_ == 1u && (key == 'f' || key == 'b' || key == 'd' || key == 127u))
                { key = key == 'f' ? key_word_right : key == 'b' ? key_word_left : key == 'd' ? key_word_delete : key_word_backspace; }
                else if(escape_ == 1u && key == 'y') { escape_ = 0u; return yank_pop(); }
                else if(escape_ == 1u && (key == '<' || key == '>'))
                {
                    escape_ = 0u;
                    if(selected_ == count_) { draft_ = line_; }
                    selected_ = key == '<' ? 0u : count_;
                    line_ = selected_ == count_ ? draft_ : history_[selected_]; cursor_ = line_.size; undo_count_ = 0u;
                    return action::changed;
                }
                else if(escape_ == 1u && (key == '[' || key == 'O'))
                { escape_ = 2u; csi_size_ = 0u; return action::unchanged; }
                else if(escape_ == 2u)
                {
                    if(key >= 0x20u && key <= 0x3fu)
                    {
                        if(csi_size_ == csi_.size()) { overflow_ = true; escape_ = 0u; return action::unchanged; }
                        csi_[csi_size_++] = static_cast<char>(key); return action::unchanged;
                    }
                    auto const parameters{::fast_io::string_view{csi_.data(), csi_size_}};
                    if(parameters.starts_with("<") && (key=='M' || key=='m'))
                    {
                        escape_=0u;  unsigned values[3]{}; ::std::size_t field{}; bool digit{}, valid{true};
                        for(char c : parameters.subview(1u))
                        {
                            if(c==';' && digit && field<2u) { ++field; digit=false; }
                            else if(c>='0' && c<='9' && values[field]<=6553u)
                            { values[field]=values[field]*10u+static_cast<unsigned>(c-'0'); digit=true; if(values[field]>65535u) { valid=false; } }
                            else { valid=false; break; }
                        }
                        // A malformed mouse report is never inserted into a command.
                        if(!valid || !digit || field!=2u || values[0]>255u || values[1]==0u || values[2]==0u) { return action::unchanged; }
                        mouse_={values[0],values[1],values[2],key=='m'}; return action::mouse;
                    }
                    bool const word{parameters == "1;5" || parameters == "1;3" || parameters == "5"};
                    bool const ordinary{parameters.empty() || parameters == "1" || parameters == "1;2"};
                    if(key == '~' && parameters == "200")
                    { escape_ = 0u; pasting_ = true; paste_end_size_ = 0u; insertion_group_ = false; return action::unchanged; }
                    if(key == '~' && (parameters == "5" || parameters == "6"))
                    { escape_ = 0u; return parameters == "5" ? action::page_up : action::page_down; }
                    if(key == '~' && (parameters == "1" || parameters == "7")) { key = key_home; }
                    else if(key == '~' && (parameters == "4" || parameters == "8")) { key = key_end; }
                    else if(key == '~' && parameters == "3") { key = key_delete; }
                    else if((ordinary || word) && key == 'C') { key = word ? key_word_right : 6u; }
                    else if((ordinary || word) && key == 'D') { key = word ? key_word_left : 2u; }
                    else if(ordinary && key == 'A') { key = key_up; }
                    else if(ordinary && key == 'B') { key = key_down; }
                    else if(ordinary && key == 'H') { key = key_home; }
                    else if(ordinary && key == 'F') { key = key_end; }
                    else { overflow_ = true; escape_ = 0u; return action::unchanged; }
                }
                else if(escape_ != 0u) { overflow_ = true; escape_ = 0u; return action::unchanged; }
                escape_ = 0u;
            }
            if(key == 27u) { escape_ = 1u; return action::unchanged; }
            if(argument_pending_)
            {
                auto const count{argument_negative_ && argument_ == 0u ? 1u : argument_}; auto const negative{argument_negative_};
                argument_pending_ = argument_negative_ = false; argument_ = 0u;
                if(negative)
                {
                    if(key == 2u) { key = 6u; } else if(key == 6u) { key = 2u; }
                    else if(key == key_word_left) { key = key_word_right; } else if(key == key_word_right) { key = key_word_left; }
                    else if(key == key_up || key == 16u) { key = key_down; } else if(key == key_down || key == 14u) { key = key_up; }
                    else { return action::unchanged; }
                }
                // Counts may edit input, but can never replay Enter, CtrlD EOF,
                // a paste delimiter, or any command execution/control operation.
                if(key == 4u && line_.size == 0u) { return action::unchanged; }
                if(key < 32u && key != 2u && key != 6u && key != 8u && key != 4u && key != 11u && key != 16u && key != 14u && key != 23u && key != 20u)
                { return action::unchanged; }
                action result{action::unchanged};
                for(unsigned i{}; i != count; ++i) { auto const value{feed(static_cast<int>(key))}; if(value == action::changed) { result = value; } }
                return result;
            }
            if(key == key_word_left) { cursor_ = word_before(cursor_); return action::changed; }
            if(key == key_word_right) { cursor_ = word_after(cursor_); return action::changed; }
            if(key == key_word_delete) { return kill(cursor_, word_after(cursor_), false); }
            if(key == key_word_backspace) { return kill(word_before(cursor_), cursor_, true); }
            if(key == 20u) { return transpose(); } // Ctrl+T, whole UTF8 characters.
            if(key == 22u) { quoted_ = true; return action::unchanged; } // bounded quoted insert.
            if(key == 24u) { escape_ = 6u; return action::unchanged; } // Ctrl+X prefix, no implicit command.
            if(key == 31u) { return undo(); } // Ctrl+_.
            if(key == key_up || key == 16u) { return recall(true); }
            if(key == key_down || key == 14u) { return recall(false); }
            if(key == key_home || key == 1u) { cursor_ = 0u; return action::changed; }
            if(key == key_end || key == 5u) { cursor_ = line_.size; return action::changed; }
            if(key == 2u) { cursor_ = previous(cursor_); return action::changed; }
            if(key == 6u) { cursor_ = next(cursor_); return action::changed; }
            if(key == 4u || key == key_delete)
            {
                if(key == 4u && line_.size == 0u) { return action::end; }
                if(cursor_ < line_.size) { save_undo(); erase(cursor_, next(cursor_)); return action::changed; }
                return action::unchanged;
            }
            if(key == 8u || key == 127u)
            { if(cursor_ == 0u) { return action::unchanged; } save_undo(); erase(previous(cursor_), cursor_); return action::changed; }
            if(key == 11u) { return kill(cursor_, line_.size, false); }
            if(key == 21u) { return kill(0u, cursor_, true); }
            if(key == 23u)
            {
                auto at{cursor_};
                while(at != 0u && (line_.bytes[previous(at)] == ' ' || line_.bytes[previous(at)] == '\t')) { at = previous(at); }
                while(at != 0u && line_.bytes[previous(at)] != ' ' && line_.bytes[previous(at)] != '\t') { at = previous(at); }
                return kill(at, cursor_, true);
            }
            if(key == 25u) { return yank(); }
            if(key < 32u || key >= key_up) { return action::unchanged; }
            return insert(key);
        }
    };
}
