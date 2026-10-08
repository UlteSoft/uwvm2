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
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_editing
{
    inline constexpr unsigned key_up{256u}, key_down{257u}, key_home{258u}, key_end{259u}, key_delete{260u}, key_word_left{261u}, key_word_right{262u}, key_word_delete{263u}, key_word_backspace{264u};
    inline constexpr ::std::size_t maximum_bytes{8448u}, maximum_history{32u}, maximum_kills{8u};
    inline constexpr unsigned maximum_argument{1024u};
    enum class action { changed, unchanged, redraw, candidates, complete, end, interrupted, oversized, invalid,
        completion, tui_toggle, tui_single, tui_split, page_up, page_down };
    struct completion_word { ::fast_io::string_view text{}; bool arguments{}; };
    // HOST command vocabulary. Argument completion is delegated to the normal
    // management thread; this editor has no filesystem or stop authority.
    inline constexpr ::fast_io::array<completion_word, 102u> completion_words{{
        {"advance", true}, {"until", true}, {"condition", true}, {"display", true}, {"info display", false}, {"undisplay", true},
        {"enable display", true}, {"disable display", true}, {"delete display", true},
        {"tui enable", false}, {"tui disable", false}, {"tui source", true},
        {"layout src", false}, {"layout asm", false}, {"layout split", false},
        {"layout regs", false}, {"layout wasm", false}, {"layout wasip1", false},
        {"layout next", false}, {"focus next", false}, {"focus prev", false},
        {"focus cmd", false}, {"focus code", false},
        {"backtrace", false},
        {"breakpoint delete", true},
        {"breakpoint list", false},
        {"breakpoint set", true},
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
        {"break", true},
        {"break-source", true},
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
        {"help", false},
        {"info breakpoints", false},
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
        {"set wasip1 arg", true},
        {"set wasip1 env", true},
        {"set wasip1 rights", true},
        {"si", false},
        {"status", false},
        {"step", false},
        {"step asm", true},
        {"step source", true},
        {"step wasm", true},
        {"table", true},
        {"trace wasm", true},
        {"unset wasip1 env", true},
        {"up", false},
        {"wait", false},
        {"wasm-script", true},
        {"where", false},
    }};
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
    struct buffer
    {
        ::fast_io::array<char, maximum_bytes> bytes{};
        ::std::size_t size{};
        [[nodiscard]] ::fast_io::string_view view() const noexcept { return ::fast_io::string_view{bytes.data(), size}; }
    };
    // Host-input editing only. Fixed owned buffers, no heap, filesystem,
    // evaluation, runtime identity, signal handler or guest/native memory.
    class editor final
    {
        buffer line_{}, draft_{}, repeat_{}, search_query_{}, search_draft_{};
        ::fast_io::array<buffer, maximum_history> history_{};
        ::fast_io::array<buffer, maximum_kills> kills_{};
        ::std::size_t cursor_{}, count_{}, selected_{};
        ::std::size_t kill_count_{}, yank_selected_{}, yank_begin_{}, yank_size_{};
        unsigned escape_{};
        ::fast_io::array<char, 16u> csi_{};
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
            if(cursor_ != line_.size) { return action::unchanged; }
            auto const prefix{line_.view()};
            ::std::size_t matches{}, common{}; completion_word const* first{};
            for(auto const& word : completion_words)
            {
                if(!word.text.starts_with(prefix)) { continue; }
                if(word.text == prefix && word.arguments && line_.size < maximum_bytes)
                {
                    save_undo(); line_.bytes[line_.size++] = ' '; cursor_ = line_.size;
                    return action::changed;
                }
                if(matches++ == 0u) { first = ::std::addressof(word); common = word.text.size(); }
                else
                {
                    ::std::size_t shared{};
                    while(shared < common && shared < word.text.size() && first->text[shared] == word.text[shared]) { ++shared; }
                    common = shared;
                }
            }
            if(first == nullptr) { return action::unchanged; }
            if(common > line_.size || (matches == 1u && first->arguments && line_.size < maximum_bytes)) { save_undo(); }
            if(common > line_.size)
            { copy(line_, first->text.subview(0u, common)); cursor_ = line_.size; }
            if(matches == 1u && first->arguments && line_.size < maximum_bytes)
            { line_.bytes[line_.size++] = ' '; cursor_ = line_.size; }
            return matches > 1u && common == prefix.size() ? action::candidates : action::changed;
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
        { for(auto const& word : completion_words) { if(word.text.starts_with(line_.view())) { display(word.text); } } }
        [[nodiscard]] buffer const& line() const noexcept { return line_; }
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
        void forget_repeat() noexcept { repeat_ = {}; }
        [[nodiscard]] action feed(int byte) noexcept
        {
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
                auto text{line_.view()}; while(text.starts_with(" ")) { text=text.subview(1u); }
                constexpr ::fast_io::array<::fast_io::string_view,18u> contexts{{"until ","advance ","display ","condition ","replace ","tui source ","break-source ",
                    "b ","break ","break-name ","print ","print-frame ","p ","ptype ","ptype-frame ","source-value ","source-type ","frame variable "}};
                for(auto prefix : contexts) { if(text.starts_with(prefix)) { return action::completion; } }
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
