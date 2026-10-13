/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include "console_memory.h"
#include "console_layout.h"
#ifndef UWVM_MODULE
# include <fast_io.h>
# include "console_stdio.h"
# include <fast_io_dsal/array.h>
# include <fast_io_dsal/string.h>
# include <fast_io_dsal/string_view.h>
# include <algorithm>
# include <cstddef>
# include <cstdint>
# include "console_line_editor.h"
# include <utility>
# include <memory>
# include <vector>
# include <span>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#  include <sys/ioctl.h>
# endif
#endif
#include "console_source_highlight.h"
#include "console_debug_highlight.h"
#include <uwvm2/utils/ansies/ansi_push_macro.h>
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::debugger::console_tui
{
    using view = ::fast_io::string_view;
    enum class layout { src, assembly, split, registers, wasm, wasip1, memory, operands, locals, io };
    inline constexpr ::fast_io::array<view, 10u> layout_names{{"src", "asm", "split", "regs", "wasm", "wasip1", "memory", "operands", "locals", "io"}};
    struct dimensions { ::std::size_t columns{80u}, rows{24u}; };
    [[nodiscard]] inline dimensions terminal_dimensions() noexcept
    {
        dimensions result{};
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
        ::winsize size{};
        if(::fast_io::posix::libc_ioctl(::fast_io::out().native_handle(), TIOCGWINSZ, ::std::addressof(size)) == 0)
        {
            // Newly allocated PTYs may report zero until their terminal host
            // supplies a size. Zero is unknown, not a one-column terminal.
            if(size.ws_col != 0u) { result.columns = size.ws_col; }
            if(size.ws_row != 0u) { result.rows = size.ws_row; }
        }
#elif defined(_WIN32) && !defined(__CYGWIN__)
        ::fast_io::win32::console_screen_buffer_info info{};
        if(::fast_io::win32::GetConsoleScreenBufferInfo(::fast_io::out().native_handle(), ::std::addressof(info)) != 0)
        { result = {static_cast<::std::size_t>(info.Window.Right-info.Window.Left+1),
                    static_cast<::std::size_t>(info.Window.Bottom-info.Window.Top+1)}; }
#endif
        result.columns = ::std::clamp(result.columns, ::std::size_t{1u}, ::std::size_t{240u});
        result.rows = ::std::clamp(result.rows, ::std::size_t{1u}, ::std::size_t{100u}); return result;
    }
    struct character { ::std::uint32_t scalar{}; ::std::size_t bytes{1u}, cells{1u}; bool valid{}; };
    [[nodiscard]] inline character decode(view text) noexcept
    {
        if(text.empty()) { return {}; }
        auto const first{static_cast<unsigned char>(text[0])};
        if(first < 128u) { return {first,1u,1u,true}; }
        ::std::size_t bytes{first>=194u && first<=223u ? 2u : first>=224u && first<=239u ? 3u : first>=240u && first<=244u ? 4u : 0u};
        if(bytes == 0u || bytes > text.size()) { return {first,1u,1u,false}; }
        ::std::uint32_t value{static_cast<::std::uint32_t>(first & (0x7fu >> bytes))};
        for(::std::size_t i{1u};i<bytes;++i)
        { auto const c{static_cast<unsigned char>(text[i])}; if((c&0xc0u)!=0x80u) { return {first,1u,1u,false}; } value=(value<<6u)|(c&0x3fu); }
        if((bytes==2u && value<128u)||(bytes==3u && value<2048u)||(bytes==4u && value<65536u)||
           value>0x10ffffu||(value>=0xd800u&&value<=0xdfffu)) { return {first,1u,1u,false}; }
        ::std::size_t cells{1u};
        if((value>=0x300u&&value<=0x36fu)||(value>=0x1ab0u&&value<=0x1affu)||
           (value>=0x1dc0u&&value<=0x1dffu)||(value>=0xfe00u&&value<=0xfe0fu)||
           (value>=0xfe20u&&value<=0xfe2fu)||value==0x200bu||value==0x200cu||value==0x200du||value==0xfeffu) { cells=0u; }
        else if((value>=0x1100u&&value<=0x115fu)||(value>=0x2329u&&value<=0x232au)||
                (value>=0x2e80u&&value<=0xa4cfu)||(value>=0xac00u&&value<=0xd7a3u)||
                (value>=0xf900u&&value<=0xfaffu)||(value>=0xfe10u&&value<=0xfe19u)||
                (value>=0xfe30u&&value<=0xfe6fu)||(value>=0xff00u&&value<=0xff60u)||
                (value>=0xffe0u&&value<=0xffe6u)||(value>=0x1f300u&&value<=0x1faffu)||
                (value>=0x20000u&&value<=0x3fffdu)) { cells=2u; }
        return {value,bytes,cells,true};
    }
    [[nodiscard]] inline ::std::size_t cells(view text) noexcept
    { ::std::size_t result{}; while(!text.empty()) { auto const c{decode(text)};result+=c.cells;text=text.subview(c.bytes); } return result; }
    [[nodiscard]] inline ::fast_io::string clean(view text, ::std::size_t cap = 32768u)
    {
        ::fast_io::string result{};
        while(!text.empty())
        {
            auto const c{decode(text)};
            if(c.valid && c.scalar>=32u && c.scalar!=127u && !(c.scalar>=128u&&c.scalar<=159u))
            { if(c.bytes>cap-result.size()) { break; } result.append(text.subview(0u,c.bytes));text=text.subview(c.bytes);continue; }
            ::fast_io::string escaped{};
            if(c.scalar=='\n' && c.valid) { escaped.push_back('\n'); }
            else if(c.scalar=='\t' && c.valid) { escaped.append("    "); }
            else if(!c.valid || c.scalar<32u || c.scalar==127u || (c.scalar>=128u&&c.scalar<=159u))
            { for(::std::size_t i{};i<c.bytes;++i) { escaped.append(::fast_io::concat_fast_io("\\x",::fast_io::mnp::hex<false,true>(static_cast<unsigned char>(text[i])))); } }
            else { escaped.append(text.subview(0u,c.bytes)); }
            if(escaped.size()>cap-result.size()) { break; }
            result.append(escaped);text=text.subview(c.bytes);
        }
        return result;
    }
    [[nodiscard]] inline ::std::size_t sgr_size(view text) noexcept
    {
        if(!text.starts_with("\x1b[")) { return 0u; }
        ::std::size_t n{2u};
        while(n < text.size() && n < 32u && ((text[n] >= '0' && text[n] <= '9') || text[n] == ';')) { ++n; }
        return n < text.size() && text[n] == 'm' ? n+1u : 0u;
    }
    [[nodiscard]] inline ::fast_io::string styled_clean(view text)
    {
        ::fast_io::string result{};
        while(!text.empty() && result.size() < 32768u)
        {
            if(auto n{sgr_size(text)}; n != 0u)
            { if(n>32768u-result.size()) { break; } result.append(text.subview(0u,n)); text=text.subview(n); }
            else
            { auto const c{decode(text)}; auto safe{clean(text.subview(0u,c.bytes))};
              if(safe.size()>32768u-result.size()) { break; } result.append(safe); text=text.subview(c.bytes); }
        }
        return result;
    }
    [[nodiscard]] inline view styled_clip(view text, ::std::size_t budget) noexcept
    {
        ::std::size_t end{}, width{};
        while(end < text.size())
        {
            if(auto n{sgr_size(text.subview(end))}; n != 0u) { end+=n; continue; }
            auto const c{decode(text.subview(end))};
            if(c.scalar == '\n' || c.cells > budget-width) { break; }
            end+=c.bytes; width+=c.cells;
        }
        return text.subview(0u,end);
    }
    [[nodiscard]] inline ::std::size_t styled_cells(view text) noexcept
    {
        ::std::size_t width{};
        while(!text.empty())
        { if(auto n{sgr_size(text)}; n != 0u) { text=text.subview(n); }
          else { auto const c{decode(text)}; width+=c.cells; text=text.subview(c.bytes); } }
        return width;
    }
    // Keep the host command on one physical row. Byte offsets in the editor
    // are UTF-8 boundaries; terminal cursor offsets are display cells.
    struct input_row { ::fast_io::string text{}; ::std::size_t column{1u}; };
    [[nodiscard]] inline view clip_cells(view text, ::std::size_t budget) noexcept
    {
        ::std::size_t end{}, width{};
        while(end < text.size())
        {
            auto const c{decode(text.subview(end))};
            if(c.scalar == '\n' || c.cells > budget - width) { break; }
            end += c.bytes; width += c.cells;
        }
        return text.subview(0u, end);
    }
    [[nodiscard]] inline input_row command_input(console_editing::editor const& editor, ::std::size_t columns)
    {
        columns = ::std::clamp(columns, ::std::size_t{1u}, ::std::size_t{240u});
        auto const budget{columns - 1u}; // Never trigger terminal autowrap.
        auto const query{clean(editor.search_query(), 32u)};
        auto const prompt_text{editor.searching() ? ::fast_io::concat_fast_io(
            editor.searching_forward()
                ? (editor.search_matched() ? view{"(i-search)`"} : view{"(failed i-search)`"})
                : (editor.search_matched() ? view{"(reverse-i-search)`"} : view{"(failed reverse-i-search)`"}),
            query, "': ") : ::fast_io::concat_fast_io("(uwvm-debug) ")};
        auto const prompt{clip_cells(prompt_text.subview(0u), budget - ::std::min(budget, ::std::size_t{4u}))};
        auto const prompt_width{cells(prompt)};
        auto const available{budget - prompt_width};
        auto const edited{clean(editor.line().view(), console_editing::maximum_bytes * 4u)};
        auto const before{clean(editor.line().view().subview(0u, editor.cursor()), console_editing::maximum_bytes * 4u)};
        auto const cursor{::std::min(before.size(), edited.size())};
        ::std::size_t first{}, width{cells(edited.subview(0u, cursor))};
        while(first < cursor && width > available)
        { auto const c{decode(edited.subview(first))}; first += c.bytes; width -= c.cells; }
        return {::fast_io::concat_fast_io(prompt, clip_cells(edited.subview(first), available)), prompt_width + width + 1u};
    }
    [[nodiscard]] inline ::fast_io::string command_line(console_editing::editor const& editor, ::std::size_t columns)
    {
        auto const row{command_input(editor, columns)};
        // CR + CUF works without the optional CSI s/u save/restore extension.
        // CUF 0 means 1 on terminals, so omit it for the first column.
        if(row.column == 1u) { return ::fast_io::concat_fast_io("\r\x1b[2K", row.text, "\r"); }
        return ::fast_io::concat_fast_io("\r\x1b[2K", row.text, "\r\x1b[", ::fast_io::mnp::dec(row.column - 1u), "C");
    }
    // Bounded ordinary source-file I/O, never a guest memory/native-code read.
    // Reject non-regular handles before reading; POSIX opens must not wait on FIFOs.
    [[nodiscard]] inline ::fast_io::string source_text(view path, ::std::uint64_t line,
        console_source_highlight::line_styles* styles = nullptr, bool intermediate = false,
        ::std::uint64_t first_line = 0u, ::std::uint64_t* total_lines = nullptr)
    {
        if(styles != nullptr) { styles->clear(); }
        if(total_lines != nullptr) { *total_lines=0u; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
            if(path.empty() || path.size() > 4096u || line == 0u) { return ::fast_io::concat_fast_io("source text unavailable\n"); }
            for(unsigned char c : path) { if(c < 32u || c == 127u) { return ::fast_io::concat_fast_io("source path unavailable\n"); } }
            auto const name{::fast_io::concat_fast_io(path)};
            // Source display only reads. Windows' default native open also asks
            // for FILE_WRITE_ATTRIBUTES, which rejects read-only media/files.
            auto mode{::fast_io::open_mode::in | ::fast_io::open_mode::no_write_attributes | ::fast_io::open_mode::shared_delete};
#if !defined(_WIN32) || defined(__CYGWIN__)
            mode |= ::fast_io::open_mode::no_block;
#endif
            // On NT no_block creates an asynchronous handle. Ordinary read_all
            // has no OVERLAPPED offset/completion state and requires a synchronous handle.
            ::fast_io::native_file file{name, mode};
            auto const before{::fast_io::status(file)};
            if(before.type != ::fast_io::file_type::regular)
            { return ::fast_io::concat_fast_io("source text unavailable: not a regular file\n"); }
            if(before.size == 0u) { return ::fast_io::concat_fast_io("source file is empty\n"); }
            ::fast_io::native_file_loader mapping{::fast_io::at(file)};
            view bytes{mapping.data(),mapping.size()}; // Map any regular source size; only viewport text is copied.
            auto const after{::fast_io::status(file)};
            if(before.dev != after.dev || before.ino != after.ino || before.size != after.size ||
               before.mtim != after.mtim || before.ctim != after.ctim)
            { return ::fast_io::concat_fast_io("source file changed during read\n"); }
            ::fast_io::string result{}; ::std::uint64_t number{1u}; ::std::size_t start{}, row_index{};
            auto const lang{console_source_highlight::detect(path)}; console_source_highlight::state lexical{};
            ::std::uint64_t total{bytes.back()=='\n' ? 0u : 1u}; for(char c : bytes) { if(c=='\n') { ++total; } }
            if(total_lines!=nullptr) { *total_lines=total; }
            auto const first{::std::min(total,first_line!=0u ? first_line : line>1u ? line-1u : 1u)};
            for(::std::size_t end{}; end <= bytes.size(); ++end)
            {
                if(end != bytes.size() && bytes[end] != '\n') { continue; }
                if(number > total) { break; }
                if(number >= first && number <= first + 99u)
                {
                    auto const prefix{::fast_io::concat_fast_io(number == line ? (intermediate ? view{"N> "} : view{"=> "}) : view{"   "},::fast_io::mnp::dec(number)," ")};
                    if(styles != nullptr) { styles->push_back({row_index++,prefix.size(),lang,lexical}); }
                    result.append(prefix); result.append(clean(bytes.subview(start,::std::min(end-start,::std::size_t{2048u})),2048u)); result.push_back('\n');
                }
                console_source_highlight::scan(bytes.subview(start,end-start),lang,lexical,
                    [](::std::size_t,::std::size_t,console_source_highlight::style) {});
                if(number > first + 99u || result.size() > 16384u) { break; }
                ++number; start = end + 1u;
            }
            return result.empty() ? ::fast_io::concat_fast_io("source line is outside the file\n") : ::std::move(result);
        }
        catch(::fast_io::error const& error)
        { return ::fast_io::concat_fast_io("source text unavailable (OS error ", ::fast_io::mnp::hex0x(error.code), "); use set source PATH\n"); }
        catch(...) { return ::fast_io::concat_fast_io("source text unavailable; use set source PATH\n"); }
#else
        return ::fast_io::concat_fast_io("source text unavailable on this build\n");
#endif
    }
    [[nodiscard]] inline ::fast_io::string source_text(::fast_io::string const& path, ::std::uint64_t line)
    { return source_text(path.subview(0u),line); }
    // Assemble styled terminal cells before emitting any bytes. Unchanged cells
    // never reach the terminal, and a changed frame is one synchronized write.
    class retained_frame
    {
        struct cell
        {
            ::fast_io::string text{" "};unsigned foreground{},background{};bool reverse{},bold{};unsigned width{1u};
            bool operator==(cell const& other) const noexcept
            { return text==other.text && foreground==other.foreground && background==other.background && reverse==other.reverse && bold==other.bold && width==other.width; }
        };
        ::std::vector<cell> next_{},previous_{};
        ::std::size_t columns_{},rows_{},x_{},y_{},cursor_x_{},cursor_y_{};
        cell style_{};bool invalid_{true};
    public:
        void invalidate() noexcept { invalid_=true; }
        void begin(dimensions size)
        { columns_=size.columns;rows_=size.rows;next_.assign(columns_*rows_,cell{});x_=y_=0u;style_=cell{}; }
        void append(view text)
        {
            while(!text.empty())
            {
                if(text.starts_with("\x1b["))
                {
                    ::std::size_t end{2u};while(end<text.size() && !(text[end]>='@' && text[end]<='~')) { ++end; }
                    if(end==text.size()) { return; }auto code{text[end]};auto args{text.subview(2u,end-2u)};
                    unsigned values[16]{},count{1u};
                    for(char c:args) { if(c>='0' && c<='9') { values[count-1u]=values[count-1u]*10u+static_cast<unsigned>(c-'0'); }
                      else if(c==';' && count<16u) { ++count; } }
                    if(code=='H') { y_=values[0] ? values[0]-1u : 0u;x_=count>1u && values[1] ? values[1]-1u : 0u; }
                    else if(code=='m') { for(unsigned i{};i<count;++i) { auto v{values[i]};
                        if(v==0u) { style_=cell{}; }else if(v==7u || v==27u) { style_.reverse=v==7u; }
                        else if(v==1u || v==22u) { style_.bold=v==1u; }
                        else if((v>=30u && v<=37u)||(v>=90u&&v<=97u)||v==39u) { style_.foreground=v==39u ? 0u : v; }
                        else if((v>=40u && v<=47u)||(v>=100u&&v<=107u)||v==49u) { style_.background=v==49u ? 0u : v; } } }
                    text=text.subview(end+1u);continue;
                }
                auto c{decode(text)};auto bytes{text.subview(0u,c.bytes)};text=text.subview(c.bytes);
                if(c.scalar<' ') { continue; }
                if(c.cells==0u) { if(x_ && y_<rows_) { auto n{x_-1u};while(n && next_[y_*columns_+n].width==0u) { --n; }next_[y_*columns_+n].text.append(bytes); }continue; }
                if(y_<rows_ && x_+c.cells<=columns_)
                {
                    // Erasing either half of a wide glyph erases the whole glyph.
                    auto at{y_*columns_+x_};if(next_[at].width==0u && x_) { next_[at-1u]=cell{}; }
                    if(next_[at].width==2u && x_+1u<columns_) { next_[at+1u]=cell{}; }
                    next_[at]=style_;next_[at].text=::fast_io::concat_fast_io(bytes);next_[at].width=static_cast<unsigned>(c.cells);
                    if(c.cells==2u) { next_[at+1u]=style_;next_[at+1u].text.clear();next_[at+1u].width=0u; }
                }
                x_+=c.cells;
            }
        }
        ::fast_io::string finish(::std::size_t cursor_row,::std::size_t cursor_column,bool clear)
        {
            bool full{invalid_ || previous_.size()!=next_.size() || clear};::fast_io::string delta{};
            if(clear) { delta.append("\x1b[r\x1b[2J\x1b[H"); }
            for(::std::size_t r{};r<rows_;++r)
            {
                ::std::size_t first{},last{columns_-1u};
                if(!full) { while(first<last && next_[r*columns_+first]==previous_[r*columns_+first]) { ++first; }
                    while(last>first && next_[r*columns_+last-1u]==previous_[r*columns_+last-1u]) { --last; } }
                if(first==last) { continue; }if(first && next_[r*columns_+first].width==0u) { --first; }
                delta.append(::fast_io::concat_fast_io("\x1b[",r+1u,";",first+1u,"H",::uwvm2::uwvm::utils::ansies::put_color ? view{"\x1b[0m"} : view{"\x1b[27m"}));cell active{};
                for(auto c{first};c<last;++c)
                { auto const& value{next_[r*columns_+c]};if(value.width==0u) { continue; }
                  if(value.foreground!=active.foreground || value.background!=active.background || value.reverse!=active.reverse || value.bold!=active.bold)
                  { delta.append("\x1b[0");if(value.foreground) { delta.append(::fast_io::concat_fast_io(";",value.foreground)); }
                    if(value.background) { delta.append(::fast_io::concat_fast_io(";",value.background)); }if(value.reverse) { delta.append(";7"); }if(value.bold) { delta.append(";1"); }delta.push_back('m');active=value; }
                  delta.append(value.text); }
            }
            previous_.swap(next_);invalid_=false;
            if(delta.empty() && cursor_x_==cursor_column && cursor_y_==cursor_row) { return {}; }
            cursor_x_=cursor_column;cursor_y_=cursor_row;
            return ::fast_io::concat_fast_io("\x1b[?2026h\x1b[?25l\x1b[?7l",delta,::uwvm2::uwvm::utils::ansies::put_color ? view{"\x1b[0m"} : view{"\x1b[27m"},"\x1b[?7h\x1b[",cursor_row,";",cursor_column,"H\x1b[?25h\x1b[?2026l");
        }
    };
    class screen final
    {
        void* context_{};
        void (*write_)(void*, view) noexcept{};
        bool terminal_{}, enabled_{}, code_focus_{true};
        dimensions rendered_size_{0u,0u};
        mutable retained_frame frame_{};mutable bool composing_{};
        layout layout_{layout::src};
        ::fast_io::array<layout, 8u> selected_{{layout::src}};
        ::std::size_t selected_count_{1u}, focused_{};
        ::fast_io::array<::std::size_t, 10u> pane_scroll_{};
        ::std::int64_t assembly_first_{},assembly_valid_first_{},assembly_lower_{-(INT64_MAX/2)},assembly_upper_{INT64_MAX/2};
        bool horizontal_{};
        bool io_follow_{true};
        mutable ::std::size_t io_tail_{};
        ::std::size_t log_scroll_{};
        ::fast_io::string log_{}, status_{}, candidates_{};
        ::fast_io::array<::fast_io::string, 10u> panels_{};
        ::fast_io::string source_override_{}, memory_query_{};
        console_source_highlight::line_styles source_styles_{};
        static constexpr ::std::size_t log_limit{32768u};
        static constexpr ::std::size_t command_pane{10u}, minimum_rows{3u}, minimum_columns{20u};
        [[nodiscard]] static constexpr ::std::size_t pane_columns(::std::size_t pane) noexcept
        { return pane==static_cast<::std::size_t>(layout::memory) ? 32u : minimum_columns; }
        struct box { ::std::size_t pane{}, left{}, top{}, width{}, height{}; };
        struct geometry
        {
            ::fast_io::array<box,9u> boxes{}; ::std::size_t count{}, code_height{}, log_start{}, log_height{};
            bool horizontal{}, valid{};
        };
        ::fast_io::array<::std::size_t,10u> weights_{};
        ::std::size_t code_height_{}, io_height_{3u};
        bool mouse_enabled_{true},content_changed_{};
        mutable ::std::size_t memory_rendered_rows_{};
        ::std::uint64_t source_first_{1u}, source_total_{1u}, source_stop_{};
        ::fast_io::string source_path_{}, fd_query_{}, pending_query_{};
        ::std::uint64_t wasip1_module_{};
        console_layout::tree tree_{};
        bool tree_active_{},custom_layout_{};
        struct point { ::std::size_t row{},column{}; };
        bool selecting_{},selection_valid_{};
        ::std::size_t selection_pane_{command_pane+1u};
        point selection_first_{},selection_last_{};
        ::fast_io::string selection_text_{},clipboard_{},tooltip_{},hover_query_{},hover_token_{};
        ::std::size_t pointer_x_{},pointer_y_{},hover_pane_{command_pane+1u};
        mutable bool drawing_tooltip_{};
        mutable ::fast_io::array<::fast_io::string,100u> painted_rows_{};
        mutable ::fast_io::array<::std::size_t,100u> painted_columns_{};
        ::std::vector<::std::byte> memory_bytes_{};
        ::std::uint64_t memory_offset_{},memory_stop_{},memory_epoch_{};
        ::std::size_t memory_first_{},memory_last_{};
        bool memory_selected_{};
        ::fast_io::string memory_format_{"all"};
        ::fast_io::string drop_hint_{};
        ::std::size_t title_drag_{command_pane+1u};point title_origin_{};bool title_resize_{};box title_box_{};
        ::std::size_t execution_drag_{command_pane+1u},execution_origin_row_{};
        [[nodiscard]] ::fast_io::string execution_target(box const& b,::std::size_t y) const
        {
            if((b.pane!=0u && b.pane!=1u && b.pane!=4u) || y<=b.top || y>=b.top+b.height) { return {}; }
            auto storage{plain(row(panels_[b.pane],pane_scroll_[b.pane]+y-b.top-1u))};auto text{storage.subview(0u)};
            if(text.size()<4u || (text.subview(0u,3u)!="=> " && text.subview(0u,3u)!="N> " && text.subview(0u,3u)!="   ")) { return {}; }
            text=text.subview(3u);auto end{text.find_character(' ')};if(end==::fast_io::containers::npos) { return {}; }
            auto token{text.subview(0u,end)};if(token.empty()) { return {}; }
            auto digits{token};bool const hex{digits.starts_with("0x")};if(hex) { digits=digits.subview(2u); }
            if(digits.empty() || (b.pane==1u)!=hex) { return {}; }
            for(char c:digits) { if(!((c>='0' && c<='9') || (hex && ((c>='a'&&c<='f') || (c>='A'&&c<='F'))))) { return {}; } }
            return ::fast_io::concat_fast_io(b.pane==0u ? view{"jump src "} : b.pane==4u ? view{"jump wasm "} : view{"jump asm *"},token);
        }
        static ::fast_io::string plain(view text)
        { ::fast_io::string result{};while(!text.empty()) { if(auto n{sgr_size(text)};n) { text=text.subview(n); }
            else { auto c{decode(text)};result.append(text.subview(0u,c.bytes));text=text.subview(c.bytes); } }return result; }
        static view slice_cells(view text,::std::size_t begin,::std::size_t end)
        { ::std::size_t first{},last{},column{};while(last<text.size()) { auto c{decode(text.subview(last))};if(column+c.cells> end) { break; }
            if(column<begin) { first=last+c.bytes; }column+=c.cells;last+=c.bytes; }return text.subview(first,last-first); }
        void make_tree()
        {
            if(tree_active_) { return; }::std::array<unsigned,8u> panes{};unsigned count{};
            for(::std::size_t i{};i<selected_count_;++i) { if(selected_[i]!=layout::io) { panes[count++]=static_cast<unsigned>(selected_[i]); } }
            tree_.chain({panes.data(),count},horizontal_);
            if(wants(layout::io)) { auto io{tree_.add({9u})};tree_.root=count ? tree_.add({10u,tree_.root,io,85u,false,false}) : io; }
            tree_active_=true;
        }
        void adopt_tree(console_layout::tree const& tree)
        { tree_=tree;tree_active_=true;selected_count_=0u;for(unsigned i{};i<tree_.count;++i) { if(tree_.nodes[i].leaf) { selected_[selected_count_++]=static_cast<layout>(tree_.nodes[i].pane); } }
          focused_=0u;layout_=selected_[0];selection_valid_=false; }
        bool move_pane(::std::size_t pane,::std::size_t target,view side)
        { make_tree();auto next{tree_};if(!next.move(static_cast<unsigned>(pane),static_cast<unsigned>(target),side)) { return false; }
          adopt_tree(next);focus_pane(pane);return true; }
        void update_selection()
        {
            auto a{selection_first_},b{selection_last_};if(a.row>b.row || (a.row==b.row && a.column>b.column)) { ::std::swap(a,b); }
            selection_text_.clear();
            for(auto r{a.row};r<=b.row && r<=painted_rows_.size();++r)
            { if(r==0u) { continue; }auto const& line{painted_rows_[r-1u]};auto const origin{painted_columns_[r-1u]};
              auto first{r==a.row && a.column>origin ? a.column-origin : 0u};auto last{r==b.row && b.column>=origin ? b.column-origin+1u : cells(line.subview(0u))};
              auto selected{slice_cells(line.subview(0u),first,last)};while(!selected.empty() && selected.back()==' ') { selected=selected.subview(0u,selected.size()-1u); }
              selection_text_.append(selected);if(r<b.row) { selection_text_.push_back('\n'); } }
        }
        void copy_selection()
        {
            if(selection_text_.empty()) { return; }clipboard_=selection_text_;
            // OSC 52 is a terminal clipboard write, never a shell command.
            constexpr char alphabet[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
            ::fast_io::string encoded{};auto text{selection_text_.subview(0u)};
            for(::std::size_t i{};i<text.size();i+=3u)
            { unsigned bits{static_cast<unsigned>(static_cast<unsigned char>(text[i]))<<16u};if(i+1u<text.size()) { bits|=static_cast<unsigned char>(text[i+1u])<<8u; }
              if(i+2u<text.size()) { bits|=static_cast<unsigned char>(text[i+2u]); }
              encoded.push_back(alphabet[(bits>>18u)&63u]);encoded.push_back(alphabet[(bits>>12u)&63u]);
              encoded.push_back(i+1u<text.size() ? alphabet[(bits>>6u)&63u] : '=');encoded.push_back(i+2u<text.size() ? alphabet[bits&63u] : '='); }
            emit(::fast_io::concat_fast_io("\x1b]52;c;",encoded,"\x07"));
#if defined(__APPLE__) && defined(__MACH__)
            // Apple Terminal does not implement OSC 52. Fixed executable and
            // pipe data keep selected source text entirely outside shell syntax.
            try { ::fast_io::native_pipe pipe;::fast_io::native_file null{"/dev/null",::fast_io::open_mode::out};
              ::fast_io::native_process child{"/usr/bin/pbcopy",::fast_io::native_process_args{}, {}, {.in=pipe,.out=null,.err=null}};
              pipe.in().close();::fast_io::io::print(pipe.out(),selection_text_);pipe.out().close();static_cast<void>(::fast_io::wait(child)); }
            catch(...) { /* The terminal clipboard protocol above remains available. */ }
#endif
        }
        [[nodiscard]] ::std::size_t memory_hit(box const& b,point p) const
        {
            if(p.row<=b.top || p.column<b.left+2u) { return SIZE_MAX; }
            auto width{console_memory::row_bytes(b.width-5u)};auto x{p.column-b.left-2u};auto r{p.row-b.top-1u+pane_scroll_[6u]};
            if(r==0u) { return SIZE_MAX; }::std::size_t byte{SIZE_MAX};
            if(x>=20u && x<20u+width*3u && (x-20u)%3u<2u) { byte=(x-20u)/3u; }
            else if(x>=22u+width*3u && x<22u+width*3u+width) { byte=x-22u-width*3u; }
            if(byte==SIZE_MAX || (r-1u)>SIZE_MAX/width) { return SIZE_MAX; }auto index{(r-1u)*width+byte};return index<memory_bytes_.size() ? index : SIZE_MAX;
        }
        void memory_tooltip()
        { if(!memory_selected_ || memory_bytes_.empty()) { return; }auto first{::std::min(memory_first_,memory_last_)},last{::std::max(memory_first_,memory_last_)};
          tooltip_=::fast_io::concat_fast_io("\x1b[90m",::fast_io::mnp::hex0x(memory_offset_+first)," · ",last-first+1u," bytes\x1b[0m\n");
          auto values{console_memory::interpret({memory_bytes_.data()+first,last-first+1u},memory_format_.subview(0u),true)};
          for(::std::size_t i{};i<rows(values);++i) { auto value{row(values,i)};auto split{value.find_character('=')};if(split==::fast_io::containers::npos) { continue; }
            tooltip_.append(::fast_io::concat_fast_io("\x1b[94m",value.subview(0u,split),"\x1b[0m = \x1b[93m",value.subview(split+1u),"\x1b[0m\n")); }
          if(!tooltip_.empty() && tooltip_.back()=='\n') { tooltip_.pop_back(); }if(!::uwvm2::uwvm::utils::ansies::put_color) { tooltip_=plain(tooltip_.subview(0u)); } }

        [[nodiscard]] geometry arrange(dimensions size) const noexcept
        {
            geometry out{};
            if(size.rows<13u || size.columns<(wants(layout::memory) ? 32u : 30u)) { return out; }
            out.valid=true;
            if(tree_active_ && tree_.count)
            {
                auto minimum{tree_.minimum(tree_.root)};
                if(size.columns>=minimum.first && size.rows-7u>=minimum.second)
                {
                    out.code_height=::std::clamp(code_height_ ? code_height_ : (size.rows-4u)*2u/3u,minimum.second,size.rows-7u);
                    tree_.visit(tree_.root,{0u,1u,3u,size.columns,out.code_height},[&](console_layout::rectangle b)
                    { out.boxes[out.count++]={b.pane,b.left,b.top,b.width,b.height}; });
                    out.log_start=3u+out.code_height;out.log_height=size.rows-out.log_start;
                    out.boxes[out.count++]={command_pane,1u,out.log_start,size.columns,out.log_height};return out;
                }
            }
            auto const automatic{::std::min(size.rows-7u,::std::max((size.rows-4u)*2u/3u,selected_count_*minimum_rows))};
            auto const has_io{wants(layout::io)};
            auto const panes{selected_count_-(has_io ? 1u : 0u)};
            auto const least{(panes ? minimum_rows : 0u)+(has_io ? minimum_rows : 0u)};
            out.code_height=::std::clamp(code_height_ ? code_height_ : automatic,least,size.rows-7u);
            auto const io_height{has_io ? (panes==0u ? out.code_height : ::std::clamp(io_height_,minimum_rows,out.code_height-minimum_rows)) : 0u};
            auto const main_height{out.code_height-io_height};
            out.horizontal=horizontal_ && size.columns>=minimum_columns*2u;
            ::fast_io::array<layout,8u> visible{}; ::std::size_t n{},first{};
            for(::std::size_t i{};i<selected_count_;++i) { if(selected_[i]!=layout::io) { visible[n++]=selected_[i]; } }
            auto const extent{out.horizontal ? size.columns : main_height};
            auto const minimum{[&](::std::size_t i) { return out.horizontal ? pane_columns(static_cast<::std::size_t>(visible[i])) : minimum_rows; }};
            auto const fitting{[&](::std::size_t begin) { ::std::size_t end{begin},used{};
                while(end<n && minimum(end)<=extent-used) { used+=minimum(end++); }return end-begin; }};
            auto count{fitting(first)};
            if(count<panes && code_focus_)
            { for(::std::size_t i{};i<n;++i) { if(visible[i]==selected_[focused_] && i>=count)
                { first=i;while(first && fitting(first-1u)>i-first+1u) { --first; }count=fitting(first);break; } } }
            ::std::size_t required{};for(::std::size_t i{};i<count;++i) { required+=minimum(first+i); }
            auto const extra{extent-required}; ::std::size_t total{};
            for(::std::size_t i{};i<count;++i) { total+=weights_[static_cast<::std::size_t>(visible[first+i])]; }
            ::std::size_t position{},allocated{};
            for(::std::size_t i{};i<count;++i)
            {
                auto const pane{static_cast<::std::size_t>(visible[first+i])};
                auto const add{i+1u==count ? extra-allocated : total ? extra*weights_[pane]/total : extra/count}; allocated+=add;
                auto const span{minimum(first+i)+add};
                out.boxes[out.count++]={pane,out.horizontal ? 1u+position : 1u,out.horizontal ? 3u : 3u+position,
                    out.horizontal ? span : size.columns,out.horizontal ? main_height : span}; position+=span;
            }
            if(has_io) { out.boxes[out.count++]={static_cast<::std::size_t>(layout::io),1u,3u+main_height,size.columns,io_height}; }
            out.log_start=3u+out.code_height; out.log_height=size.rows-out.log_start;
            out.boxes[out.count++]={command_pane,1u,out.log_start,size.columns,out.log_height}; return out;
        }
        [[nodiscard]] static view pane_name(::std::size_t pane) noexcept
        { return pane==command_pane ? view{"cmd"} : layout_names[pane]; }
        [[nodiscard]] static ::std::size_t pane_id(view name) noexcept
        {
            if(name=="cmd" || name=="commands") { return command_pane; }
            name=console_layout::pane_alias(name);
            for(::std::size_t i{};i<layout_names.size();++i) { if(name==layout_names[i]) { return i; } }
            return command_pane+1u;
        }
        [[nodiscard]] static view drop_side(box const& target,::std::size_t column,::std::size_t row) noexcept
        { auto x{column-target.left},y{row-target.top};auto dx{::std::min(x,target.width-1u-x)},dy{::std::min(y,target.height-1u-y)};
          return dx*target.height<=dy*target.width ? (x*2u<target.width ? view{"left"} : view{"right"}) : (y*2u<target.height ? view{"above"} : view{"below"}); }
        void resize_title(console_editing::mouse_event event,dimensions size,geometry const& plan)
        {
            bool horizontal{plan.horizontal && title_drag_!=command_pane && title_drag_!=9u};bool reverse{title_drag_==command_pane || title_drag_==9u};
            if(tree_active_ && title_drag_!=command_pane)
            { for(unsigned i{};i<tree_.count;++i) { auto const& n{tree_.nodes[i]};if(n.leaf) { continue; }
                bool first{tree_.nodes[n.first].leaf && tree_.nodes[n.first].pane==title_drag_};bool second{tree_.nodes[n.second].leaf && tree_.nodes[n.second].pane==title_drag_};
                if(first || second) { horizontal=n.horizontal;reverse=second;break; } } }
            else if(title_drag_!=command_pane && title_drag_!=9u)
            { auto count{plan.count-1u-(wants(layout::io) ? 1u : 0u)};reverse=count>1u && plan.boxes[count-1u].pane==title_drag_; }
            auto delta{static_cast<int>(horizontal ? event.column : event.row)-static_cast<int>(horizontal ? title_origin_.column : title_origin_.row)};
            if(reverse) { delta=-delta; }
            static_cast<void>(resize_pane(title_drag_,static_cast<::std::size_t>(::std::max(1,static_cast<int>(horizontal ? title_box_.width : title_box_.height)+delta)),size));
        }
        bool resize_pane(::std::size_t pane, ::std::size_t requested, dimensions size)
        {
            auto const plan{arrange(size)}; if(!plan.valid) { return false; }
            if(pane==command_pane)
            { auto const least{selected_count_==1u ? minimum_rows : 2u*minimum_rows};
              auto const height{::std::clamp(requested,::std::size_t{4u},size.rows-3u-least)};
              code_height_=size.rows-3u-height; return true; }
            if(tree_active_) { return tree_.resize(static_cast<unsigned>(pane),requested,{0u,1u,3u,size.columns,plan.code_height}); }
            for(::std::size_t i{};i<plan.count;++i)
            {
                auto const& current{plan.boxes[i]}; if(current.pane!=pane) { continue; }
                if(pane==static_cast<::std::size_t>(layout::io))
                { if(i==0u) { code_height_=::std::clamp(requested,minimum_rows,size.rows-7u); }
                  else { io_height_=::std::clamp(requested,minimum_rows,plan.code_height-minimum_rows); } return true; }
                auto const main_count{plan.count-1u-(wants(layout::io) ? 1u : 0u)};
                if(main_count<2u) { return false; }
                auto const neighbor{i+1u<main_count ? i+1u : i-1u};
                auto const dimension{[&](box const& b) { return plan.horizontal ? b.width : b.height; }};
                auto const minimum{[&](box const& b) { return plan.horizontal ? pane_columns(b.pane) : minimum_rows; }};
                auto const total{dimension(current)+dimension(plan.boxes[neighbor])};
                auto const desired{::std::clamp(requested,minimum(current),total-minimum(plan.boxes[neighbor]))};
                for(::std::size_t j{};j<main_count;++j) { weights_[plan.boxes[j].pane]=dimension(plan.boxes[j])-minimum(plan.boxes[j]); }
                weights_[pane]=desired-minimum(current); weights_[plan.boxes[neighbor].pane]=total-desired-minimum(plan.boxes[neighbor]); return true;
            }
            return false;
        }
        void scroll_pane(::std::size_t pane, bool down, ::std::size_t amount)
        {
            if(pane==static_cast<::std::size_t>(layout::src))
            {
                auto const previous{source_first_};
                source_first_=down ? source_first_+::std::min(source_total_-source_first_,static_cast<::std::uint64_t>(amount)) :
                    source_first_>amount ? source_first_-amount : 1u;
                content_changed_|=source_first_!=previous;return;
            }
            if(pane==static_cast<::std::size_t>(layout::assembly))
            {
                auto const delta{static_cast<::std::int64_t>(::std::min(amount,static_cast<::std::size_t>(INT64_MAX/2)))};
                auto const previous{assembly_first_};
                assembly_first_=::std::clamp(assembly_first_+(down ? delta : -delta),assembly_lower_,assembly_upper_);
                content_changed_|=assembly_first_!=previous;return;
            }
            if(pane==static_cast<::std::size_t>(layout::io) && io_follow_)
            { pane_scroll_[pane]=io_tail_; io_follow_=false; }
            auto& value{pane==command_pane ? log_scroll_ : pane_scroll_[pane]};
            auto const limit{pane==command_pane ? rows(log_) : pane==6u && !memory_bytes_.empty() ? memory_rendered_rows_ : rows(panels_[pane])};
            bool const increment{pane==command_pane ? !down : down};
            value=increment ? ::std::min(value+amount,limit>1u ? limit-1u : 0u) : value>amount ? value-amount : 0u;
        }
        void focus_pane(::std::size_t pane)
        {
            if(pane==command_pane) { code_focus_=false; return; }
            for(::std::size_t i{};i<selected_count_;++i)
            { if(static_cast<::std::size_t>(selected_[i])==pane) { focused_=i; code_focus_=true; return; } }
        }
        void set_mouse_reporting(bool enabled)
        {
            console_editing::mouse_reporting=enabled;
            emit(enabled ? view{"\x1b[?1003h\x1b[?1006h"} : view{"\x1b[?1003l\x1b[?1002l\x1b[?1006l"});
            if(!enabled) { title_drag_=command_pane+1u; }
        }
        void emit(view text) const noexcept { if(composing_) { frame_.append(text); }else { write_(context_, text); } }
        void emit(::fast_io::string const& text) const noexcept { emit(text.subview(0u)); }
        [[nodiscard]] static view row(view text, ::std::size_t index) noexcept
        {
            ::std::size_t b{};
            while(index != 0u)
            { auto const n{text.find_character('\n', b)}; if(n == ::fast_io::containers::npos) { return {}; } b=n+1u; --index; }
            auto const n{text.find_character('\n', b)}; return text.subview(b, n == ::fast_io::containers::npos ? text.size()-b : n-b);
        }
        [[nodiscard]] static view row(::fast_io::string const& text, ::std::size_t index) noexcept { return row(text.subview(0u),index); }
        [[nodiscard]] static ::std::size_t rows(view text) noexcept
        { if(text.empty()) { return 0u; }::std::size_t n{text.back()=='\n' ? 0u : 1u}; for(char c : text) { if(c == '\n') { ++n; } } return n; }
        [[nodiscard]] static ::std::size_t rows(::fast_io::string const& text) noexcept { return rows(text.subview(0u)); }
        template<::std::size_t N> void color(char const (&ansi)[N]) const
        { emit(::fast_io::concat_fast_io(::uwvm2::uwvm::utils::ansies::diagnostic_color(ansi))); }
        void styled(view text) const
        {
            while(!text.empty())
            {
                if(auto n{sgr_size(text)}; n!=0u)
                { if(::uwvm2::uwvm::utils::ansies::put_color) { emit(text.subview(0u,n)); } text=text.subview(n); }
                else
                {
                    auto run_end{text.find_character('\x1b')};
                    if(run_end == ::fast_io::containers::npos) { run_end=text.size(); }
                    // Stored text contains only printable data and complete SGR.
                    // Emit a run at once, avoiding a syscall per source character.
                    if(run_end == 0u) { run_end=1u; }
                    emit(text.subview(0u,run_end)); text=text.subview(run_end);
                }
            }
        }
        void paint(view text, console_source_highlight::style style) const
        {
            using s=console_source_highlight::style;
            switch(style)
            {
                case s::keyword: color(UWVM_AES_LT_PURPLE); break;
                case s::type: color(UWVM_AES_LT_BLUE); break;
                case s::number: color(UWVM_AES_LT_CYAN); break;
                case s::string: color(UWVM_AES_LT_GREEN); break;
                case s::comment: color(UWVM_AES_GRAY); break;
                default: color(UWVM_AES_WHITE); break;
            }
            styled(text);
        }
        void draw_row(::std::size_t number, view text, ::std::size_t columns,
            console_source_highlight::style style = console_source_highlight::style::plain, ::std::size_t column = 1u) const
        {
            emit(::fast_io::concat_fast_io("\x1b[",::fast_io::mnp::dec(number),";",::fast_io::mnp::dec(column),"H"));
            paint(clip_cells(text,columns > 0u ? columns-1u : 0u),style);
            for(auto n{cells(clip_cells(text,columns > 0u ? columns-1u : 0u))}; n+1u<columns; ++n) { emit(" "); } color(UWVM_AES_RST_ALL);
        }
        void draw_row(::std::size_t number, ::fast_io::string const& text, ::std::size_t columns,
            console_source_highlight::style style = console_source_highlight::style::plain) const
        { draw_row(number,text.subview(0u),columns,style); }
        void border(::std::size_t number, view title, ::std::size_t columns, bool active, ::std::size_t column = 1u) const
        {
            auto value{::fast_io::concat_fast_io("+- [",title,active ? view{" *] "} : view{"] "})};
            while(cells(value.subview(0u))+2u < columns) { value.push_back('-'); } value.push_back('+');
            draw_row(number,value.subview(0u),columns,active ? console_source_highlight::style::keyword : console_source_highlight::style::comment,column);
        }
        void body(::std::size_t number, view text, ::std::size_t columns,
            console_source_highlight::line_style const* source = nullptr, ::std::size_t column = 1u, bool instructions=false, bool assembly=false, bool source_pane=false) const
        {
            if(columns < 6u) { draw_row(number,text,columns,console_source_highlight::style::plain,column); return; }
            auto const clipped{styled_clip(text,columns-5u)};
            if(!drawing_tooltip_ && number<=painted_rows_.size() && (!selection_valid_ || (column<=selection_first_.column && selection_first_.column<column+columns)))
            { painted_rows_[number-1u]=plain(clipped);painted_columns_[number-1u]=column+2u; }
            emit(::fast_io::concat_fast_io("\x1b[",::fast_io::mnp::dec(number),";",::fast_io::mnp::dec(column),"H"));
            color(UWVM_AES_GRAY); emit("| ");
            auto prefix{source != nullptr ? source->prefix : 0u};
            if(source_pane && (text.starts_with("=> ") || text.starts_with("N> ") || text.starts_with("   ")))
            {
                ::std::size_t end{3u};while(end<text.size() && text[end]>='0' && text[end]<='9') { ++end; }
                if(end>3u && end<text.size() && text[end]==' ') { prefix=::std::min(end+1u,clipped.size()); }
            }
            if(prefix != 0u && prefix <= clipped.size())
            {
                if(text.starts_with("=> ") || text.starts_with("N> ")) { color(UWVM_AES_YELLOW); } else { color(UWVM_AES_GRAY); }
                emit(clipped.subview(0u,prefix));
                auto const code{clipped.subview(prefix)};
                if(source == nullptr) { paint(code,console_source_highlight::style::plain); }
                else
                {
                    auto state{source->before};
                    // Merge adjacent lexical runs before writing SGR/output.
                    ::std::size_t first{}, last{}; auto previous{console_source_highlight::style::plain}; bool pending{};
                    auto flush=[&] { if(pending) { paint(code.subview(first,last-first),previous); } };
                    console_source_highlight::scan(code,source->lang,state,[&](::std::size_t begin,::std::size_t end,console_source_highlight::style style)
                    {
                        if(pending && previous == style && last == begin) { last=end; }
                        else { flush(); first=begin; last=end; previous=style; pending=true; }
                    }); flush();
                }
            }
            else if(instructions)
            { console_debug_highlight::scan(clipped,assembly,[&](::std::size_t begin,::std::size_t end,console_source_highlight::style style)
              { paint(clipped.subview(begin,end-begin),style); }); }
            else if(clipped.starts_with("error:")) { color(UWVM_AES_LT_RED); styled(clipped); }
            else if(clipped.starts_with("(uwvm-debug) ")) { color(UWVM_AES_LT_PURPLE); styled(clipped.subview(0u,13u)); color(UWVM_AES_WHITE);styled(clipped.subview(13u)); }
            else if(clipped.starts_with("Usage:") || clipped.starts_with("Example:")) { color(UWVM_AES_LT_CYAN); styled(clipped); }
            else { paint(clipped,console_source_highlight::style::plain); }
            if(selection_valid_ && !drawing_tooltip_)
            {
                auto a{selection_first_},b{selection_last_};if(a.row>b.row || (a.row==b.row && a.column>b.column)) { ::std::swap(a,b); }
                if(number>=a.row && number<=b.row && column<=selection_first_.column && selection_first_.column<column+columns)
                { auto text{plain(clipped)};auto first{number==a.row && a.column>column+2u ? a.column-column-2u : 0u};
                  auto last{number==b.row && b.column>=column+2u ? b.column-column-1u : cells(text.subview(0u))};
                  auto selected{slice_cells(text.subview(0u),first,last)};
                  emit(::fast_io::concat_fast_io("\x1b[",number,";",column+2u+first,"H\x1b[7m",selected,"\x1b[27m"));
                  emit(::fast_io::concat_fast_io("\x1b[",number,";",column+2u+styled_cells(clipped),"H")); }
            }
            color(UWVM_AES_GRAY);
            for(auto count{styled_cells(clipped)+4u}; count+1u < columns; ++count) { emit(" "); }
            emit(" |"); color(UWVM_AES_RST_ALL);
        }
        void panel(::std::size_t start, ::std::size_t height, layout which, dimensions size,
            ::std::size_t column = 1u) const
        {
            auto const selected{static_cast<::std::size_t>(which)};
            border(start,layout_names[selected],size.columns,code_focus_ && selected_[focused_] == which,column);
            ::fast_io::string memory_text{};
            if(which==layout::memory && !memory_bytes_.empty())
            { memory_text=::fast_io::concat_fast_io("Wasm memory | select bytes; memory format ",memory_format_,"\n",
                console_memory::dump(memory_bytes_,memory_offset_,size.columns-5u)); }
            if(which==layout::memory) { memory_rendered_rows_=rows(memory_text); }
            auto const content{memory_text.empty() ? panels_[selected].subview(0u) : memory_text.subview(0u)};
            auto first{pane_scroll_[selected]};
            if(which == layout::io)
            {
                auto const count{rows(panels_[selected])};
                auto const visible{height>0u?height-1u:0u};
                io_tail_=count>visible ? count-visible : 0u;
                if(io_follow_) { first=io_tail_; }
            }
            for(::std::size_t i{1u}; i<height; ++i)
            {
                auto const index{first+i-1u}; console_source_highlight::line_style const* style{};
                if(which == layout::src) { for(auto const& line : source_styles_) { if(line.row == index) { style=::std::addressof(line); break; } } }
                body(start+i,row(content,index),size.columns,style,column,which==layout::wasm || which==layout::assembly,which==layout::assembly,which==layout::src);
                if(which==layout::memory && memory_selected_ && index>0u && !memory_bytes_.empty())
                {
                    auto width{console_memory::row_bytes(size.columns-5u)},begin{(index-1u)*width};
                    auto low{::std::min(memory_first_,memory_last_)},high{::std::max(memory_first_,memory_last_)};
                    if(begin<=high && begin+width>low)
                    {
                        auto first{::std::max(begin,low)-begin},last{::std::min({begin+width-1u,high,memory_bytes_.size()-1u})-begin};
                        auto line{row(content,index)};auto highlight{[&](::std::size_t x,::std::size_t count)
                        { if(x>=size.columns-5u) { return; }count=::std::min(count,size.columns-5u-x);
                          auto text{slice_cells(line,x,x+count)};emit(::fast_io::concat_fast_io("\x1b[",start+i,";",column+2u+x,"H\x1b[7m",text,"\x1b[27m")); }};
                        highlight(20u+first*3u,(last-first)*3u+2u);highlight(22u+width*3u+first,last-first+1u);
                    }
                }
            }
        }
        void render_candidates(::std::size_t start, ::std::size_t height, dimensions size) const
        {
            if(height == 0u) { return; }
            border(start,"Tab completion: next key clears",size.columns,true);
            auto const count{rows(candidates_)};
            for(::std::size_t i{1u}; i<height; ++i)
            {
                if(i+1u == height && count > height-1u) { body(start+i,"... type a longer prefix to narrow matches",size.columns); }
                else { body(start+i,row(candidates_,i-1u),size.columns); }
            }
        }

    public:
        screen(void* context, void (*write)(void*,view) noexcept, bool terminal) noexcept
            : context_{context}, write_{write}, terminal_{terminal} { select(layout::src); }
        screen(screen const&) = delete;
        screen& operator=(screen const&) = delete;
        ~screen() { disable(); }
        bool input_key(console_editing::editor& editor,int key)
        {
            if(!enabled_) { return false; }
            if((key==3 || key==-3) && selection_valid_ && !selection_text_.empty()) { copy_selection();return true; }
            if(key!=22) { return false; }
            auto text{clipboard_};
#if defined(__APPLE__) && defined(__MACH__)
            try
            {
                ::fast_io::native_pipe pipe;::fast_io::native_file null{"/dev/null",::fast_io::open_mode::in|::fast_io::open_mode::out};
                ::fast_io::native_process child{"/usr/bin/pbpaste",::fast_io::native_process_args{}, {}, {.in=null,.out=pipe,.err=null}};
                pipe.out().close();text.clear();char bytes[1024];bool oversized{};
                for(;;) { auto end{::fast_io::operations::read_some(pipe.in(),bytes,bytes+sizeof(bytes))};if(end==bytes) { break; }
                    auto n{static_cast<::std::size_t>(end-bytes)};if(n>console_editing::maximum_bytes-text.size()) { oversized=true; }
                    else if(!oversized) { text.append(bytes,n); } }
                static_cast<void>(::fast_io::wait(child));if(oversized) { output("Clipboard exceeds the command length limit.\n");return true; }
            }
            catch(...) { text=clipboard_; }
#endif
            // Clipboard bytes are input data. No control/escape sequence can
            // submit commands; multiline content requires a separate Enter.
            for(auto& c:text) { if(static_cast<unsigned char>(c)<32u || c==127) { c=' '; } }
            auto result{editor.replace_completion(editor.cursor(),editor.cursor(),text.subview(0u))};
            if(result==console_editing::action::unchanged && !text.empty()) { output("Clipboard text cannot fit in this command.\n"); }
            return true;
        }
        [[nodiscard]] bool needs_resize(dimensions size=terminal_dimensions()) const noexcept
        { return size.columns!=rendered_size_.columns || size.rows!=rendered_size_.rows; }
        [[nodiscard]] bool enabled() const noexcept { return enabled_; }
        [[nodiscard]] bool has_candidates() const noexcept { return !candidates_.empty(); }
        [[nodiscard]] bool custom_layout() const noexcept { return custom_layout_; }
        [[nodiscard]] layout current_layout() const noexcept { return layout_; }
        [[nodiscard]] bool take_content_change() noexcept { return ::std::exchange(content_changed_,false); }
        [[nodiscard]] view memory_query() const noexcept { return memory_query_.subview(0u); }
        [[nodiscard]] view source_override() const noexcept { return source_override_.subview(0u); }
        void enable() { if(terminal_ && !enabled_) { dismiss_candidates(); frame_.invalidate();enabled_=true; console_stdio::enabled.store(true,::std::memory_order_release); emit("\x1b[?1049h\x1b[2J\x1b[H"); set_mouse_reporting(mouse_enabled_); } }
        void disable() noexcept { dismiss_candidates(); if(enabled_) { drain_guest(); set_mouse_reporting(false); enabled_=false; console_stdio::enabled.store(false,::std::memory_order_release); color(UWVM_AES_RST_ALL); emit("\x1b[r\x1b[?25h\x1b[?1049l"); } }
        // Line mode appends bounded candidates below the command, like a shell.
        // TUI keeps its existing in-pane overlay and never enters scrollback.
        void show_candidates(view text)
        {
            if(!terminal_) { return; }
            candidates_=clean(text,16384u);
            if(!enabled_ && !candidates_.empty())
            {
                emit("\r\n");
                auto const width{terminal_dimensions().columns};
                ::std::size_t used{};
                for(::std::size_t i{}; i<rows(candidates_); ++i)
                {
                    auto const item{row(candidates_,i)};
                    auto const n{cells(item)};
                    if(used != 0u && used+n+2u>=width) { emit("\r\n"); used=0u; }
                    emit(item); emit("  "); used+=n+2u;
                }
                emit("\r\n"); candidates_.clear();
            }
        }
        void dismiss_candidates() noexcept { candidates_.clear(); }
        void record(view text)
        {
            auto safe{styled_clean(text)};
            if(safe.size() >= log_limit) { log_=::std::move(safe); }
            else
            { if(log_.size() > log_limit-safe.size()) { auto cut{log_.find_character('\n',log_.size()-(log_limit-safe.size()))};
                log_=cut == ::fast_io::containers::npos ? ::fast_io::string{} : ::fast_io::concat_fast_io(log_.subview(cut+1u)); }
              log_.append(safe); }
            log_scroll_=0u;
        }
        void output(view text) { dismiss_candidates(); record(text); if(!enabled_) { emit(text); } }
        void record(::fast_io::string const& text) { record(text.subview(0u)); }
        void output(::fast_io::string const& text) { output(text.subview(0u)); }
        void set_status(::fast_io::string const& text) { set_status(text.subview(0u)); }
        void set_panel(layout which, ::fast_io::string const& text) { set_panel(which,text.subview(0u)); }
        void set_status(view text) { status_=clean(text, 2048u); }
        void set_panel(layout which, view text) { panels_[static_cast<::std::size_t>(which)]=clean(text); }
        // Only console-owned formatters pass SGR; metadata is escaped before
        // interpolation. Ordinary pane data continues to use set_panel.
        void set_value_panel(layout which, view text)
        { if(which==layout::locals || which==layout::operands || which==layout::registers)
          { auto const index{static_cast<::std::size_t>(which)};panels_[index]=styled_clean(text);
            if(which==layout::registers) { auto const count{rows(panels_[index])};pane_scroll_[index]=::std::min(pane_scroll_[index],count ? count-1u : 0u); } } }
        void set_source_styles(console_source_highlight::line_styles styles, ::std::size_t first_row)
        { source_styles_=::std::move(styles); for(auto& value : source_styles_) { value.row+=first_row; } }
        void clear_panels() { for(::std::size_t i{}; i<panels_.size(); ++i) { if(i != static_cast<::std::size_t>(layout::io)) { panels_[i].clear(); } } source_styles_.clear(); }
        [[nodiscard]] bool wants(layout which) const noexcept
        { for(::std::size_t i{}; i<selected_count_; ++i) { if(selected_[i] == which) { return true; } } return false; }
        void select(layout which)
        {
            tree_active_=false;selection_valid_=false;
            for(auto& weight : weights_) { weight=0u; } title_drag_=command_pane+1u;
            layout_=which; selected_[0]=which; selected_count_=1u; focused_=0u;
            if(which == layout::split || which == layout::registers)
            { selected_[0]=which == layout::split ? layout::src : layout::assembly;
              selected_[1]=which == layout::split ? layout::assembly : layout::registers; selected_count_=2u; }
            if(!wants(layout::io)) { selected_[selected_count_++]=layout::io; }
            for(auto& value : pane_scroll_) { value=0u; }
        }
        bool drain_guest()
        {
            auto text{console_stdio::take()}; if(text.empty()) { return false; }
            guest_output(view{text.data(),text.size()}); return true;
        }
        void guest_output(view text)
        {
            auto& log{panels_[static_cast<::std::size_t>(layout::io)]};
            auto safe{clean(text,16384u)};
            if(log.size()+safe.size()>log_limit) { log.clear(); }
            if(!log.empty() && log.back()!='\n') { log.push_back('\n'); }
            log.append(safe);
            io_follow_=true;
            if(!enabled_) { emit(text); }
        }
        [[nodiscard]] bool command(view command)
        {
            auto word{[&]() noexcept
            {
                ::std::size_t begin{};
                while(begin < command.size() && ::fast_io::char_category::c_space::char_is(command[begin])) { ++begin; }
                auto end{begin};
                while(end < command.size() && !::fast_io::char_category::c_space::char_is(command[end])) { ++end; }
                auto const result{command.subview(begin, end - begin)};
                while(end < command.size() && ::fast_io::char_category::c_space::char_is(command[end])) { ++end; }
                command = command.subview(end); return result;
            }};
            auto const name{word()};
            if(name=="selection")
            { auto sub{word()};if(sub=="range")
              {
                  auto pane{pane_id(word())};::std::array<unsigned,4u> values{};bool valid{pane<=command_pane};
                  for(auto& value:values) { auto token{word()};auto p{::fast_io::parse_by_scan(token.begin(),token.end(),::fast_io::mnp::dec_get<true,true>(value))};valid=valid && !token.empty() && p.code==::fast_io::parse_code::ok && p.iter==token.end() && value!=0u; }
                  auto size{terminal_dimensions()};auto plan{arrange(size)};box const* found{};
                  for(::std::size_t i{};i<plan.count;++i) { if(plan.boxes[i].pane==pane) { found=&plan.boxes[i];break; } }
                  if(!enabled_ || !valid || !command.empty() || !found || values[0]>=found->height || values[2]>=found->height || values[1]>found->width-5u || values[3]>found->width-5u)
                  { output("error: selection range PANE ROW COLUMN ROW COLUMN uses visible body cells, starting at 1\n"); }
                  else { mouse({0u,static_cast<unsigned>(found->left+1u+values[1]),static_cast<unsigned>(found->top+values[0]),false},size);
                    mouse({32u,static_cast<unsigned>(found->left+1u+values[3]),static_cast<unsigned>(found->top+values[2]),false},size);
                    mouse({0u,static_cast<unsigned>(found->left+1u+values[3]),static_cast<unsigned>(found->top+values[2]),true},size); }return true;
              }
              if(sub=="clear" && command.empty()) { selection_valid_=false;selection_text_.clear(); }
              else if(sub=="copy" && command.empty()) { copy_selection(); }
              else if(sub=="show" && command.empty()) { output(selection_text_);output("\n"); }
              else { output("error: selection copy|show|clear|range PANE ROW COLUMN ROW COLUMN\n"); }return true; }
            if(name=="io")
            {
                auto sub{word()};drain_guest();
                if(sub=="clear" && command.empty()) { console_stdio::clear();panels_[9u].clear();pane_scroll_[9u]=0u;io_tail_=0u; }
                else if(sub=="show" && command.empty()) { if(!panels_[9u].empty()) { output(::fast_io::concat_fast_io(panels_[9u],panels_[9u].back()=='\n' ? view{} : view{"\n"})); } }
                else if(sub=="input") { if(!console_stdio::queue_input(command)) { output("error: guest stdin queue exceeds 32768 bytes\n"); } }
                else if(sub=="follow" && (command=="on" || command=="off")) { io_follow_=command=="on"; }
                else if(sub=="save" && !command.empty())
                { try { ::fast_io::native_file file{::fast_io::concat_fast_io(command),::fast_io::open_mode::out};::fast_io::io::print(file,panels_[9u]);output("IO output saved.\n"); }
                  catch(...) { output("error: cannot save IO output\n"); } }
                else { output("error: io clear|show|save PATH|input TEXT|follow on|off\n"); }return true;
            }
            if(name=="window")
            {
                custom_layout_=true;
                auto const sub{word()}; auto const size{terminal_dimensions()};
                if(sub=="reset" && command.empty())
                { for(auto& w : weights_) { w=0u; } for(auto& n : tree_.nodes) { n.ratio=50u; } code_height_=0u;io_height_=3u;title_drag_=command_pane+1u;return true; }
                if(sub=="resize")
                {
                    auto const pane{pane_id(word())}; auto const token{word()}; ::std::size_t extent{};
                    auto parsed{::fast_io::parse_by_scan(token.cbegin(),token.cend(),::fast_io::mnp::dec_get<true,true>(extent))};
                    if(pane>command_pane || token.empty() || parsed.code!=::fast_io::parse_code::ok || parsed.iter!=token.cend() || !command.empty() || extent>65535u ||
                       !resize_pane(pane,extent,size))
                    { output("error: window resize PANE CELLS requires a visible pane and an adjacent pane; minimum 3 rows / 20 columns (cmd 4 rows)\n"); return true; }
                }
                else if(!((sub.empty() || sub=="show") && command.empty()))
                { output("error: window show|reset|resize PANE CELLS\n"); return true; }
                auto const plan{arrange(size)};
                output("windows: minimum 3 rows / 20 columns; cmd minimum 4 rows\n");
                for(::std::size_t i{};i<plan.count;++i)
                { auto const& b{plan.boxes[i]};output(::fast_io::concat_fast_io("window ",pane_name(b.pane)," x=",b.left," y=",b.top," width=",b.width," height=",b.height,"\n")); }
                return true;
            }
            if(name=="scroll")
            {
                auto const pane{pane_id(word())};auto const direction{word()};auto const token{word()};::std::size_t amount{3u};bool valid{true};
                if(!token.empty()) { auto parsed{::fast_io::parse_by_scan(token.cbegin(),token.cend(),::fast_io::mnp::dec_get<true,true>(amount))};
                    valid=parsed.code==::fast_io::parse_code::ok && parsed.iter==token.cend(); }
                if(pane>command_pane || (direction!="up" && direction!="down") || !valid || amount==0u || amount>4096u || !command.empty())
                { output("error: scroll PANE up|down [LINES] (1..4096)\n");return true; }
                scroll_pane(pane,direction=="down",amount);return true;
            }
            if(name == "input")
            {
                if(!console_stdio::queue_input(command)) { output("error: guest stdin queue exceeds 32768 bytes\n"); }
                else { output("Guest stdin queued (one line). Empty queue reads as EOF.\n"); }
                return true;
            }
            if(name=="tui")
            { auto sub{word()};
              if(sub=="disable" && command.empty()) { disable(); }
              else if(sub=="enable" && command.empty()) { if(terminal_) { enable(); }else { output("error: TUI requires an interactive terminal\n"); } }
              else { output("error: tui enable|disable\n"); }return true; }
            if(name=="mouse" || name=="info" || name=="memory" || name=="set")
            {
                auto sub{name};
                if(name=="info") { if(word()!="wasip1" || word()!="fd") { return false; }sub="fd"; }
                if(name=="set") { if(word()!="source") { return false; }sub="source"; }
                if(sub=="mouse")
                { auto const value{word()}; if((value!="on" && value!="off") || !command.empty()) { output("error: mouse on|off\n"); }
                  else { mouse_enabled_=value=="on"; if(enabled_) { set_mouse_reporting(mouse_enabled_); } } return true; }
                if(sub=="fd")
                {
                    auto const module{word()}, descriptor{word()}; ::std::uint64_t m{},fd{};
                    auto decimal{[] (view t,::std::uint64_t& v) { auto p{::fast_io::parse_by_scan(t.cbegin(),t.cend(),::fast_io::mnp::dec_get<true,true>(v))};return !t.empty() && p.code==::fast_io::parse_code::ok && p.iter==t.cend(); }};
                    if(module=="all" && descriptor.empty() && command.empty()) { fd_query_.clear();pane_scroll_[static_cast<::std::size_t>(layout::wasip1)]=0u;return true; }
                    if(!command.empty()) { return false; }
                    if(!decimal(module,m) || !decimal(descriptor,fd)) { output("error: info wasip1 fd MODULE FD | info wasip1 fd all\n");return true; }
                    wasip1_module_=m;fd_query_=::fast_io::concat_fast_io("info wasip1 fd ",m," ",fd);pending_query_=fd_query_;
                    pane_scroll_[static_cast<::std::size_t>(layout::wasip1)]=0u;return true;
                }
                if(sub == "memory")
                {
                    auto remaining{command};auto operation{word()};command=remaining;
                    if(operation=="format")
                    { static_cast<void>(word());auto type{word()};if(!console_memory::valid_type(type) || !command.empty()) { output("error: unknown memory format; use all char wc utf8 utf16 utf32 i8 u8 i16 u16 i32 u32 i64 u64 f32 f64\n"); }
                      else { memory_format_=::fast_io::concat_fast_io(type);memory_tooltip(); }return true; }
                    if(operation=="select")
                    { static_cast<void>(word());auto offset_text{word()},count_text{word()};::std::uint64_t offset{},count{};
                      auto a{::fast_io::parse_by_scan(offset_text.begin(),offset_text.end(),::fast_io::mnp::dec_get<true,true>(offset))};
                      auto b{::fast_io::parse_by_scan(count_text.begin(),count_text.end(),::fast_io::mnp::dec_get<true,true>(count))};
                      if(offset_text.empty() || count_text.empty() || a.code!=::fast_io::parse_code::ok || a.iter!=offset_text.end() || b.code!=::fast_io::parse_code::ok || b.iter!=count_text.end() || !command.empty() || offset<memory_offset_ || offset-memory_offset_>=memory_bytes_.size() || !count || count>memory_bytes_.size()-(offset-memory_offset_))
                      { output("error: memory select OFFSET COUNT requires bytes in the current Wasm memory view\n"); }
                      else { memory_first_=static_cast<::std::size_t>(offset-memory_offset_);memory_last_=memory_first_+static_cast<::std::size_t>(count)-1u;memory_selected_=true;memory_tooltip();output(::fast_io::concat_fast_io(tooltip_,"\n")); }return true; }

                    ::std::uint64_t values[4]{}; bool valid{true};
                    for(auto& value : values)
                    { auto const token{word()}; auto scan{::fast_io::parse_by_scan(token.cbegin(),token.cend(),::fast_io::mnp::dec_get<true,true>(value))};
                      valid &= !token.empty() && scan.code==::fast_io::parse_code::ok && scan.iter==token.cend(); }
                    if(!valid || !command.empty() || values[1]>0xffffffffu || values[3]==0u || values[3]>256u || values[2]>UINT64_MAX-values[3])
                    { output("error: memory MODULE MEMORY OFFSET COUNT (COUNT 1..256)\n"); return true; }
                    memory_bytes_.clear();memory_selected_=false;tooltip_.clear();
                    memory_query_=::fast_io::concat_fast_io("memory ",values[0]," ",values[1]," ",values[2]," ",values[3]);pending_query_=memory_query_;return true;
                }
                if(sub == "source" && !command.empty())
                { if(command.size() > 4096u) { output("error: source path exceeds 4096 bytes\n"); }
                  else { source_override_=::fast_io::concat_fast_io(command); } return true; }
                output("error: mouse on|off | info wasip1 fd MODULE FD | set source PATH | memory MODULE MEMORY OFFSET COUNT\n"); return true;
            }
            if(name == "layout")
            {
                auto const previous_custom_layout{custom_layout_};
                custom_layout_=true;
                auto remaining{command};auto operation{word()};command=remaining;
                if(operation=="add" || operation=="remove")
                {
                    static_cast<void>(word());auto pane{pane_id(word())};auto side{word()};auto target{side.empty() ? static_cast<::std::size_t>(selected_[focused_]) : pane_id(word())};
                    if(pane>=command_pane || pane==2u || !command.empty() || (operation=="remove" && !side.empty())) { output("error: layout add PANE [left|right|above|below TARGET] | layout remove PANE\n");return true; }
                    make_tree();auto next{tree_};
                    if(operation=="remove")
                    { if(!next.remove(static_cast<unsigned>(pane))) { output("error: cannot remove the last pane or a missing pane\n");return true; } }
                    else
                    { if(!next.insert(static_cast<unsigned>(pane),static_cast<unsigned>(target),side.empty() ? view{"below"} : side)) { output("error: duplicate pane, invalid target/side or eight-pane limit\n");return true; } }
                    adopt_tree(next);if(operation=="add") { focus_pane(pane); }return true;
                }
                if(operation=="config")
                {
                    custom_layout_=previous_custom_layout;
                    static_cast<void>(word());auto action{word()};
                    if((action!="save" && action!="load") || command.empty()) { output("error: layout config save <filePath> | layout config load <filePath>\n");return true; }
                    try
                    {
                        if(action=="save")
                        { make_tree();::fast_io::native_file file{::fast_io::concat_fast_io(command),::fast_io::open_mode::out};
                          ::fast_io::io::print(file,"uwvm-layout 1\n",tree_.serialize(),"\n",code_height_,"\n");output("Layout configuration saved.\n"); }
                        else
                        { auto mode{::fast_io::open_mode::in};
#if defined(__unix__) || defined(__APPLE__)
                          mode|=::fast_io::open_mode::no_block;
#endif
                          ::fast_io::native_file file{::fast_io::concat_fast_io(command),mode};
                          if(::fast_io::status(file).type!=::fast_io::file_type::regular) { output("error: layout config requires a regular file\n");return true; }
                          ::fast_io::native_file_loader data{::fast_io::at(file)};
                          view text{data.data(),data.size()};auto version{row(text,0u)},description{row(text,1u)},height{row(text,2u)};
                          ::std::size_t value{};auto parsed{::fast_io::parse_by_scan(height.begin(),height.end(),::fast_io::mnp::dec_get<true,true>(value))};console_layout::tree next{};
                          if(data.size()>4096u || version!="uwvm-layout 1" || !next.parse(description) || height.empty() || parsed.code!=::fast_io::parse_code::ok || parsed.iter!=height.end() || value>65535u || rows(text)>4u || !row(text,3u).empty())
                          { output("error: invalid layout configuration; current layout retained\n"); }
                          else { adopt_tree(next);custom_layout_=true;code_height_=value;output("Layout configuration loaded.\n"); } }
                    }
                    catch(...) { output("error: cannot read or write layout configuration; current layout retained\n"); }return true;
                }
                if(operation=="move")
                { static_cast<void>(word());auto pane{pane_id(word())};auto side{word()};auto target{pane_id(word())};
                  if(!command.empty() || !move_pane(pane,target,side)) { output("error: layout move PANE left|right|above|below TARGET (visible panes)\n"); }return true; }
                if(operation=="tree")
                { static_cast<void>(word());console_layout::tree next{};if(!next.parse(command)) { output("error: layout tree h50(src,v50(wasm,memory)); unique panes, at most 8\n"); }
                  else { adopt_tree(next);if(terminal_) { enable(); } }return true; }
                ::fast_io::array<layout,8u> next{}; ::std::size_t count{};
                bool horizontal{horizontal_};
                auto requested{word()};
                if(requested == "horizontal" || requested == "vertical")
                { horizontal=requested == "horizontal"; requested=word(); }
                if(requested == "next" && command.empty())
                { auto next{(static_cast<unsigned>(layout_)+1u)%layout_names.size()};if(next==2u) { ++next; }select(static_cast<layout>(next)); }
                else
                {
                    do
                    {
                        requested=console_layout::pane_alias(requested);
                        ::std::size_t i{}; while(i<layout_names.size() && requested!=layout_names[i]) { ++i; }
                        if(i==layout_names.size() || i==2u || count==next.size())
                        { output("error: layout [horizontal|vertical] src|wasm|asm|regs|locals|stack|memory|wasip1|io ... (up to 8 panes)\n"); return true; }
                        auto const which{static_cast<layout>(i)};
                        bool duplicate{}; for(::std::size_t j{}; j<count; ++j) { duplicate|=next[j]==which; } if(!duplicate) { next[count++]=which; }
                        requested=word();
                    } while(!requested.empty());
                    for(auto& weight : weights_) { weight=0u; } title_drag_=command_pane+1u;
                    tree_active_=false;selection_valid_=false;selected_=next; selected_count_=count; layout_=next[0]; focused_=0u;
                    if(!wants(layout::io) && selected_count_<selected_.size()) { selected_[selected_count_++]=layout::io; }
                    for(auto& value : pane_scroll_) { value=0u; }
                }
                horizontal_=horizontal;
                if(terminal_) { enable(); } else { output("error: TUI requires an interactive terminal\n"); } return true;
            }
            if(name == "focus")
            {
                auto target{word()};
                if(!command.empty()) { output("error: focus next|prev|cmd|code|PANE\n"); return true; }
                if(target == "cmd") { code_focus_=false; }
                else if(target == "code") { code_focus_=true; }
                else if(target == "next" || target == "prev")
                {
                    auto index{code_focus_ ? focused_ : selected_count_};
                    index=target == "next" ? (index+1u)%(selected_count_+1u) : (index+selected_count_)%(selected_count_+1u);
                    code_focus_=index<selected_count_; if(code_focus_) { focused_=index; }
                }
                else
                {
                    target=console_layout::pane_alias(target);
                    bool found{};
                    for(::std::size_t i{}; i<selected_count_; ++i)
                    { if(target == layout_names[static_cast<::std::size_t>(selected_[i])]) { focused_=i; code_focus_=true; found=true; break; } }
                    if(!found) { output("error: focus requires a visible pane, cmd, code, next or prev\n"); }
                }
                return true;
            }
            return false;
        }
        void prepare_source(view path, ::std::uint64_t line, ::std::uint64_t stop)
        {
            if(source_stop_!=stop || source_path_.subview(0u)!=path)
            { source_stop_=stop; source_path_=::fast_io::concat_fast_io(path); source_first_=line>1u ? line-1u : 1u; pane_scroll_[0]=0u; }
        }
        [[nodiscard]] ::std::uint64_t source_first_line() const noexcept { return source_first_; }
        void source_bounds(::std::uint64_t total) noexcept { source_total_=::std::max(total,::std::uint64_t{1u}); source_first_=::std::min(source_first_,source_total_); }
        [[nodiscard]] ::std::int64_t assembly_first() const noexcept { return assembly_first_; }
        [[nodiscard]] ::std::int64_t assembly_valid_first() const noexcept { return assembly_valid_first_; }
        void assembly_position(::std::int64_t first) noexcept
        { assembly_first_=assembly_valid_first_=first;pane_scroll_[1u]=0u; }
        void assembly_bound(bool lower,::std::int64_t at) noexcept
        { if(lower) { assembly_lower_=at; }else { assembly_upper_=at; } }
        [[nodiscard]] ::std::size_t pane_body_rows(layout which,dimensions size=terminal_dimensions()) const
        {
            auto const plan{arrange(size)};
            for(::std::size_t i{};i<plan.count;++i)
            { if(plan.boxes[i].pane==static_cast<::std::size_t>(which)) { return plan.boxes[i].height>1u ? plan.boxes[i].height-1u : 1u; } }
            return 16u;
        }
        void wasip1_module(::std::uint64_t module) noexcept { wasip1_module_=module; }
        void observe_stop(::std::uint64_t stop,::std::uint64_t epoch=0u)
        { if(memory_stop_!=stop || memory_epoch_!=epoch) { memory_stop_=stop;memory_epoch_=epoch;assembly_first_=assembly_valid_first_=0;assembly_lower_=-(INT64_MAX/2);assembly_upper_=INT64_MAX/2;pane_scroll_[1u]=0u;execution_drag_=command_pane+1u;drop_hint_.clear();memory_bytes_.clear();memory_selected_=false;selection_valid_=false;tooltip_.clear();hover_token_.clear(); } }
        void memory_snapshot(::std::span<::std::byte const> bytes,::std::uint64_t offset)
        { if(offset!=memory_offset_ || bytes.size()!=memory_bytes_.size() || !::std::equal(bytes.begin(),bytes.end(),memory_bytes_.begin()))
            { memory_selected_=false; }memory_offset_=offset;memory_bytes_.assign(bytes.begin(),bytes.end()); }
        [[nodiscard]] ::fast_io::string take_hover_query() { auto query{::std::move(hover_query_)};hover_query_.clear();return query; }
        void hover_reply(view text) { while(!text.empty() && text.back()=='\n') { text=text.subview(0u,text.size()-1u); }tooltip_=styled_clean(text); }
        [[nodiscard]] view fd_query() const noexcept { return fd_query_.subview(0u); }
        [[nodiscard]] ::fast_io::string take_query() { auto query{::std::move(pending_query_)}; pending_query_.clear(); return query; }
        void mouse(console_editing::mouse_event event, dimensions size=terminal_dimensions())
        {
            if(!enabled_ || !mouse_enabled_) { return; }auto const plan{arrange(size)};
            if(!plan.valid || event.column==0u || event.row==0u || event.column>size.columns || event.row>size.rows) { return; }
            pointer_x_=event.column;pointer_y_=event.row;box const* hit{};
            for(::std::size_t i{};i<plan.count;++i) { auto const& b{plan.boxes[i]};if(event.column>=b.left && event.column<b.left+b.width && event.row>=b.top && event.row<b.top+b.height) { hit=&b;break; } }
            if(execution_drag_<=command_pane)
            {
                auto query{hit && hit->pane==execution_drag_ ? execution_target(*hit,event.row) : ::fast_io::string{}};
                drop_hint_=query.empty() ? ::fast_io::string{} : ::fast_io::concat_fast_io("Move execution: ",query);
                if(event.released)
                { if(!query.empty() && event.row!=execution_origin_row_) { pending_query_=::std::move(query); }
                  execution_drag_=command_pane+1u;drop_hint_.clear(); }
                return;
            }
            if(title_drag_<=command_pane)
            {
                drop_hint_.clear();
                // A horizontal drag along the title row resizes without Shift
                // (some terminals reserve Shift for native text selection).
                bool along_title{event.row==title_origin_.row && event.column!=title_origin_.column &&
                    title_drag_!=command_pane && title_drag_!=9u && plan.horizontal};
                if(tree_active_ && title_drag_!=command_pane && event.row==title_origin_.row && event.column!=title_origin_.column)
                { for(unsigned i{};i<tree_.count;++i) { auto const& n{tree_.nodes[i]};if(n.leaf) { continue; }
                    if((tree_.nodes[n.first].leaf && tree_.nodes[n.first].pane==title_drag_) ||
                       (tree_.nodes[n.second].leaf && tree_.nodes[n.second].pane==title_drag_)) { along_title=n.horizontal;break; } } }
                if(title_resize_ || along_title)
                { title_resize_=true;resize_title(event,size,plan);content_changed_=true;drop_hint_=::fast_io::concat_fast_io("Resize ",pane_name(title_drag_)); }
                else if(hit && hit->pane!=title_drag_ && hit->pane!=command_pane && title_drag_!=command_pane)
                { drop_hint_=::fast_io::concat_fast_io("Move ",pane_name(title_drag_)," ",drop_side(*hit,event.column,event.row)," of ",pane_name(hit->pane)); }
                if(event.released)
                {
                    if(title_resize_ || (hit && hit->pane==title_drag_)) { resize_title(event,size,plan); }
                    else if(hit && hit->pane!=command_pane && title_drag_!=command_pane)
                    { static_cast<void>(move_pane(title_drag_,hit->pane,drop_side(*hit,event.column,event.row))); }
                    title_drag_=command_pane+1u;drop_hint_.clear();
                }
                return;
            }
            if(selecting_)
            {
                if(hit && hit->pane==selection_pane_ && event.row>hit->top)
                { selection_last_={event.row,::std::clamp(static_cast<::std::size_t>(event.column),hit->left+2u,hit->left+hit->width-4u)};
                  if(selection_pane_==6u) { auto byte{memory_hit(*hit,selection_last_)};if(byte!=SIZE_MAX) { memory_last_=byte;memory_tooltip(); } }
                  update_selection(); }
                if(event.released) { selecting_=false;if(selection_first_.row!=selection_last_.row || selection_first_.column!=selection_last_.column) { copy_selection(); }
                  else if(hit && hit->pane==5u)
                  { auto text{row(panels_[5u],pane_scroll_[5u]+event.row-hit->top-1u)};while(!text.empty() && text.front()==' ') { text=text.subview(1u); }
                    if(text.starts_with("fd=")) { text=text.subview(3u);auto end{text.find_character(' ')};if(end==::fast_io::containers::npos) { end=text.size(); }
                      ::std::uint64_t fd{};auto p{::fast_io::parse_by_scan(text.begin(),text.begin()+end,::fast_io::mnp::dec_get<true,true>(fd))};
                      if(end && p.code==::fast_io::parse_code::ok && p.iter==text.begin()+end) { fd_query_=::fast_io::concat_fast_io("info wasip1 fd ",wasip1_module_," ",fd);pending_query_=fd_query_; } } }
                }return;
            }
            if(!hit) { tooltip_.clear();hover_token_.clear();return; }
            if((event.button&64u)!=0u) { selection_valid_=false;tooltip_.clear();focus_pane(hit->pane);scroll_pane(hit->pane,(event.button&1u)!=0u,3u);return; }
            if(event.released) { return; }
            if((event.button&32u)!=0u && (event.button&3u)==3u)
            {
                auto previous_tooltip{::std::move(tooltip_)};tooltip_.clear();
                if(event.row<=hit->top || hit->pane==command_pane) { hover_token_.clear();return; }
                auto text{row(panels_[hit->pane],pane_scroll_[hit->pane]+event.row-hit->top-1u)};
                auto cleaned{plain(text)};text=cleaned.subview(0u);auto cell{event.column>hit->left+2u ? event.column-hit->left-2u : 0u};
                auto prefix{slice_cells(text,0u,cell)};auto at{prefix.size()};if(at>=text.size()) { hover_token_.clear();return; }
                auto identifier{[](char c) { return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='$'; }};
                if(!identifier(text[at])) { hover_token_.clear();return; }
                auto first{at},last{at};while(first && identifier(text[first-1u])) { --first; }while(last<text.size() && identifier(text[last])) { ++last; }
                auto token{text.subview(first,last-first)};
                if(token.empty() || (hit->pane==0u && token.front()>='0' && token.front()<='9')) { hover_token_.clear();return; }
                if(hit->pane==0u)
                { if(token!=hover_token_.subview(0u) || hover_pane_!=hit->pane) { hover_query_=::fast_io::concat_fast_io("src ",token); } }
                else if(hit->pane==6u) { auto byte{memory_hit(*hit,{event.row,event.column})};if(byte!=SIZE_MAX) { memory_first_=memory_last_=byte;memory_selected_=true;memory_tooltip(); } }
                else if(hit->pane==1u || hit->pane==3u)
                { if(token!=hover_token_.subview(0u) || hover_pane_!=hit->pane) { hover_query_=::fast_io::concat_fast_io("asm ",token); } }
                else if(hit->pane==4u)
                {
                    bool queried{};
                    for(view op:{view{"local.get "},view{"local.set "},view{"local.tee "}})
                    { auto begin{::std::search(text.begin(),text.end(),op.begin(),op.end())};if(begin==text.end()) { continue; }
                      auto first{begin+op.size()},last{first};while(last!=text.end() && *last>='0' && *last<='9') { ++last; }
                      if(first!=last) { hover_query_=::fast_io::concat_fast_io("wasm local ",view{first,static_cast<::std::size_t>(last-first)});queried=true; }break; }
                    static_cast<void>(queried);
                }
                else if(hit->pane==7u || hit->pane==8u)
                {
                    if(text.starts_with("["))
                    { auto last{text.find_character(']')};if(last!=::fast_io::containers::npos && last>1u)
                      { auto index{text.subview(1u,last-1u)};bool numeric{true};for(char c:index) { numeric &= c>='0' && c<='9'; }
                        if(numeric) { hover_query_=::fast_io::concat_fast_io(hit->pane==8u ? view{"wasm local "} : view{"wasm stack "},index); } } }
                }
                if((hit->pane==0u || hit->pane==1u || hit->pane==3u) && token==hover_token_.subview(0u) && hover_pane_==hit->pane) { tooltip_=::std::move(previous_tooltip); }
                hover_token_=::fast_io::concat_fast_io(token);hover_pane_=hit->pane;return;
            }
            if((event.button&3u)!=0u || (event.button&32u)!=0u) { return; }
            focus_pane(hit->pane);tooltip_.clear();hover_token_.clear();
            auto title_width{(hit->pane==command_pane ? view{"commands"} : pane_name(hit->pane)).size()+4u};
            if(event.row==hit->top && event.column>=hit->left+3u && event.column<hit->left+3u+title_width)
            { custom_layout_=true;title_drag_=hit->pane;title_origin_={event.row,event.column};title_resize_=(event.button&4u)!=0u;title_box_=*hit;selection_valid_=false;return; }
            if(event.row<=hit->top) { return; }
            if((hit->pane==0u || hit->pane==1u || hit->pane==4u) && event.column>=hit->left+2u && event.column<hit->left+4u)
            {
                auto text{plain(row(panels_[hit->pane],pane_scroll_[hit->pane]+event.row-hit->top-1u))};
                if(text.subview(0u).starts_with("=> ") && !execution_target(*hit,event.row).empty())
                { execution_drag_=hit->pane;execution_origin_row_=event.row;selection_valid_=false;return; }
            }
            selection_pane_=hit->pane;selection_first_=selection_last_={event.row,::std::clamp(static_cast<::std::size_t>(event.column),hit->left+2u,hit->left+hit->width-4u)};
            selecting_=selection_valid_=true;selection_text_.clear();
            // Snapshot this pane before subsequent render passes; neighbor columns
            // cannot overwrite the text which is being selected.
            auto content{hit->pane==command_pane ? log_.subview(0u) : panels_[hit->pane].subview(0u)};
            ::fast_io::string memory_text{};if(hit->pane==6u && !memory_bytes_.empty()) { memory_text=::fast_io::concat_fast_io("Wasm memory\n",console_memory::dump(memory_bytes_,memory_offset_,hit->width-5u));content=memory_text.subview(0u); }
            auto first{hit->pane==command_pane ? (rows(log_)>hit->height-1u+log_scroll_ ? rows(log_)-(hit->height-1u)-log_scroll_ : 0u) : hit->pane==9u && io_follow_ ? io_tail_ : pane_scroll_[hit->pane]};
            for(::std::size_t r{1u};r<hit->height && hit->top+r<=painted_rows_.size();++r)
            { painted_rows_[hit->top+r-1u]=plain(styled_clip(row(content,first+r-1u),hit->width-5u));painted_columns_[hit->top+r-1u]=hit->left+2u; }
            if(hit->pane==6u) { auto byte{memory_hit(*hit,selection_first_)};memory_selected_=byte!=SIZE_MAX;if(memory_selected_) { memory_first_=memory_last_=byte;memory_tooltip(); } }
        }
        void key(console_editing::action key)
        {
            using action=console_editing::action;
            if(key==action::idle) { return; }
            if(key!=action::mouse && key!=action::unchanged) { execution_drag_=command_pane+1u;drop_hint_.clear();tooltip_.clear();hover_token_.clear(); }
            if(key != action::candidates) { dismiss_candidates(); }
            if(key==action::redraw) { frame_.invalidate(); }
            if(key == action::tui_toggle) { if(enabled_) { disable(); } else { enable(); } }
            else if(key == action::tui_single) { select(layout::src); enable(); }
            else if(key == action::tui_split) { select(layout::split); enable(); }
            else if(enabled_ && (key == action::page_up || key == action::page_down))
            {
              scroll_pane(code_focus_ ? static_cast<::std::size_t>(selected_[focused_]) : command_pane,key==action::page_down,5u); }

        }
        void render(console_editing::editor const& editor, dimensions size = terminal_dimensions())
        {
            if(!enabled_) { return; }
            bool const resized{needs_resize(size)};frame_.begin(size);composing_=true;
            if(resized)
            { rendered_size_=size;execution_drag_=command_pane+1u;drop_hint_.clear();selection_valid_=selecting_=false;title_drag_=command_pane+1u;tooltip_.clear();hover_token_.clear();
              }
            emit("\x1b[?25l\x1b[r");
            if(size.rows < 13u || size.columns < 30u)
            { for(::std::size_t i{1u}; i<size.rows; ++i) { draw_row(i,view{},size.columns); }
              draw_row(1u, "TUI: enlarge terminal or Ctrl+X A", size.columns); }
            else
            {
                draw_row(1u, ::fast_io::concat_fast_io("UWVM debugger | layout ", layout_names[static_cast<unsigned>(layout_)],
                    " | Ctrl+X A toggle | PgUp/PgDn"), size.columns,console_source_highlight::style::keyword);
                draw_row(2u, drop_hint_.empty() ? status_.subview(0u) : drop_hint_.subview(0u), size.columns,console_source_highlight::style::number);
                auto const plan{arrange(size)};
                for(::std::size_t i{};i+1u<plan.count;++i)
                { auto const& box{plan.boxes[i]}; panel(box.top,box.height,static_cast<layout>(box.pane),{box.width,size.rows},box.left); }
                auto const log_start{plan.log_start}, height{plan.log_height};
                if(has_candidates()) { render_candidates(log_start,height,size); }
                else
                {
                    border(log_start,"commands",size.columns,!code_focus_);
                    auto const count{rows(log_)};auto const visible{height>0u?height-1u:0u};
                    auto const offset{count>visible+log_scroll_ ? count-visible-log_scroll_ : 0u};
                    for(::std::size_t i{1u}; i<height; ++i) { body(log_start+i,row(log_,offset+i-1u),size.columns); }
                }
            }
            if(!tooltip_.empty() && size.columns>=30u && size.rows>=13u)
            {
                ::std::size_t content_width{12u};for(::std::size_t i{};i<rows(tooltip_);++i) { content_width=::std::max(content_width,styled_cells(row(tooltip_,i))); }
                auto width{::std::min({content_width+5u,::std::size_t{80u},size.columns-2u})},height{::std::min(rows(tooltip_)+1u,size.rows-4u)};
                auto left{::std::min(pointer_x_+1u,size.columns-width)},top{::std::min(pointer_y_+1u,size.rows-height)};
                left=::std::max(left,::std::size_t{1u});top=::std::max(top,::std::size_t{3u});border(top,"inspect",width,true,left);
                drawing_tooltip_=true;for(::std::size_t i{1u};i<height;++i) { body(top+i,row(tooltip_,i-1u),width,nullptr,left); }drawing_tooltip_=false;
            }
            auto const input{command_input(editor, size.columns)};
            draw_row(size.rows, input.text, size.columns, console_source_highlight::style::plain);
            composing_=false;auto delta{frame_.finish(size.rows,input.column,resized)};if(!delta.empty()) { emit(delta); }
        }
    };
}

#include <uwvm2/utils/ansies/ansi_pop_macro.h>
